open Lwt.Syntax

(* Diagnostic counters *)
let connection_count = ref 0
let message_count = ref 0
let last_log_time = ref 0.0

type pending_send = {
  payload : Yojson.Basic.t;
  timestamp : float;
}

type channel = {
  websocket : Dream.websocket;
  mutable current_subscription : string option;
  mutable pending_sends : pending_send list;
  mutable send_in_progress : bool;
}

let max_pending_sends = 2

type mutation_result =
  | Ack of (unit, string) result
  | NoAck

type broadcast_fn = string -> (string -> string) -> unit Lwt.t

type t = {
  adapter : Adapter.packed;
  resolve_subscription : Dream.request -> string -> string option Lwt.t;
  load_snapshot : Dream.request -> string -> string Lwt.t;
  handle_mutation : (broadcast_fn -> Dream.request -> action_id:string -> Yojson.Basic.t -> mutation_result Lwt.t) option;
  validate_mutation : (Dream.request -> Yojson.Basic.t -> (unit, string) result Lwt.t) option;
  handle_media : (broadcast_fn -> Dream.request -> string -> string -> (unit, string) result Lwt.t) option;
  handle_disconnect : (broadcast_fn -> string -> unit Lwt.t) option;
  channels : (string, channel list) Hashtbl.t;
}

let log_stats t =
  let now = Unix.gettimeofday () in
  if now -. !last_log_time > 10.0 then begin
    let num_channels = Hashtbl.length t.channels in
    Printf.eprintf "[stats] connections=%d channels=%d messages=%d\n%!"
      !connection_count num_channels !message_count;
    last_log_time := now;
    message_count := 0
  end

let create ~adapter ~resolve_subscription ~load_snapshot ?handle_mutation ?validate_mutation ?handle_media ?handle_disconnect () =
  {
    adapter;
    resolve_subscription;
    load_snapshot;
    handle_mutation;
    validate_mutation;
    handle_media;
    handle_disconnect;
    channels = Hashtbl.create 32;
  }

let json_string value = Yojson.Basic.to_string value

let wrap_patch message =
  let payload =
    try Yojson.Basic.from_string message with _ -> `String message
  in
  `Assoc
    [
      ("type", `String "patch");
      ("timestamp", `Float (Unix.gettimeofday () *. 1000.0));
      ("payload", payload);
    ]
  |> json_string

let wrap_snapshot message =
  let payload =
    try Yojson.Basic.from_string message with _ -> `String message
  in
  `Assoc [ ("type", `String "snapshot"); ("payload", payload) ] |> json_string

let ack_message ~action_id ~status ?error () =
  let fields = [ ("type", `String "ack"); ("actionId", `String action_id); ("status", `String status) ] in
  let fields =
    match error with
    | Some error -> ("error", `String error) :: fields
    | None -> fields
  in
  `Assoc fields |> json_string

let pong_message = `Assoc [ ("type", `String "pong") ] |> json_string

let make_broadcast_fn t target wrap =
  let wrapped = wrap "" in
  let send_start = Unix.gettimeofday () in
  match Hashtbl.find_opt t.channels target with
  | Some subscribers ->
      Lwt_list.iter_p
        (fun { websocket; _ } ->
          Lwt.catch
            (fun () ->
              let* () = Dream.send websocket wrapped in
              let send_time = Unix.gettimeofday () -. send_start in
              if send_time > 0.005 then
                Printf.eprintf "[send] channel=%s took %.3fs len=%d\n%!" target send_time
                  (String.length wrapped);
              Lwt.return_unit)
            (fun exn ->
              Printf.eprintf "[ws] send to channel %s failed: %s\n%!" target
                (Printexc.to_string exn);
              Lwt.return_unit))
        subscribers
  | None -> Lwt.return_unit

let send_to_channel t channel message =
  make_broadcast_fn t channel (fun _ -> message)

let broadcast t channel ?(wrap = wrap_patch) message =
  send_to_channel t channel (wrap message)

let unsubscribe_channel t channel =
  (* Call handle_disconnect BEFORE removing from adapter *)
  let* () = match t.handle_disconnect with
    | Some handler ->
        handler (make_broadcast_fn t) channel
    | None -> Lwt.return_unit
  in
  Hashtbl.remove t.channels channel;
  Adapter.unsubscribe t.adapter ~channel

let remove_websocket_from_channel t channel websocket =
  match Hashtbl.find_opt t.channels channel with
  | None -> Lwt.return_unit
  | Some subscribers ->
      let remaining =
        subscribers
        |> List.filter (fun subscriber -> subscriber.websocket != websocket)
      in
      if List.length remaining = List.length subscribers then
        Lwt.return_unit
      else if remaining = [] then
        unsubscribe_channel t channel
      else begin
        Hashtbl.replace t.channels channel remaining;
        Lwt.return_unit
      end

let channels_for_websocket t websocket =
  Hashtbl.fold
    (fun channel subscribers acc ->
      if List.exists (fun subscriber -> subscriber.websocket == websocket) subscribers
      then channel :: acc
      else acc)
    t.channels []

let remove_websocket_from_other_channels t websocket ~except =
  channels_for_websocket t websocket
  |> List.filter (fun channel -> channel <> except)
  |> Lwt_list.iter_s (fun channel -> remove_websocket_from_channel t channel websocket)

let detach_websocket t websocket =
  decr connection_count;
  Printf.eprintf "[ws] connection closed, total=%d\n%!" !connection_count;
  let channels = channels_for_websocket t websocket in
  let* () =
    Lwt_list.iter_s
      (fun channel ->
        Printf.eprintf "[ws] detach channel=%s\n%!" channel;
        let* () = remove_websocket_from_channel t channel websocket in
        Printf.eprintf "[ws] detach complete for channel=%s\n%!" channel;
        Lwt.return_unit)
      channels
  in
  Lwt.pause ()

let subscribe_websocket t request websocket channel =
  Printf.eprintf "[ws] subscribe channel=%s\n%!" channel;
  let* () = remove_websocket_from_other_channels t websocket ~except:channel in
  let existing_subscribers =
    match Hashtbl.find_opt t.channels channel with
    | Some subscribers -> subscribers
    | None -> []
  in
  let already_subscribed =
    List.exists (fun subscriber -> subscriber.websocket == websocket) existing_subscribers
  in
  let subscribers =
    if already_subscribed then
      existing_subscribers
    else
      {
        websocket;
        current_subscription = Some channel;
        pending_sends = [];
        send_in_progress = false;
      }
      :: existing_subscribers
  in
  let () = Hashtbl.replace t.channels channel subscribers in
  let* snapshot = t.load_snapshot request channel in
  let* () =
    if existing_subscribers = [] then
      Adapter.subscribe t.adapter ~channel ~handler:(broadcast t channel)
    else
      Lwt.return_unit
  in
  let* snapshot_sent =
    Lwt.catch
      (fun () ->
        let* () = Dream.send websocket (wrap_snapshot snapshot) in
        Lwt.return_true)
      (fun exn ->
        Printf.eprintf "[ws] failed to send snapshot: %s\n%!" (Printexc.to_string exn);
        Lwt.return_false)
  in
  if snapshot_sent then
    Lwt.return (Some channel)
  else begin
    let* () = remove_websocket_from_channel t channel websocket in
    Lwt.return_none
  end

let assoc_string key = function
  | `Assoc fields -> (
      match List.assoc_opt key fields with
      | Some (`String value) -> Some value
      | _ -> None)
  | _ -> None

let assoc_json key = function
  | `Assoc fields -> List.assoc_opt key fields
  | _ -> None

let handle_json_message_with_io t request current_channel json ~send ~close
    ~subscribe =
  incr message_count;
  log_stats t;
  let msg_type = assoc_string "type" json in
  Printf.eprintf "[ws] message type: %s\n%!" (match msg_type with Some t -> t | None -> "unknown");
  match msg_type with
  | Some "ping" ->
    let* () = send pong_message in
    Lwt.return current_channel
  | Some "select" -> (
    match assoc_string "subscription" json with
    | Some selection ->
      let* channel = t.resolve_subscription request selection in
      (match channel with
       | Some channel ->
         let* new_channel = subscribe channel in
         (match new_channel with
          | Some ch -> Lwt.return (Some ch)
          | None -> Lwt.return current_channel)
       | None -> Lwt.return current_channel)
    | None -> Lwt.return current_channel)
  | Some "mutation" -> (
      match (assoc_string "actionId" json, assoc_json "action" json) with
      | Some action_id, Some action -> (
          match t.validate_mutation with
          | Some validate ->
              let* validation = validate request action in
              (match validation with
              | Error error ->
                  let* () = send (ack_message ~action_id ~status:"error" ~error ()) in
                  Lwt.return current_channel
              | Ok () ->
                  (match t.handle_mutation with
                  | Some handler ->
                      let* result = handler (make_broadcast_fn t) request ~action_id action in
                      (match result with
                      | Ack (Ok ()) ->
                          let* () = send (ack_message ~action_id ~status:"ok" ()) in
                          Lwt.return current_channel
                       | Ack (Error error) ->
                           let* () = send (ack_message ~action_id ~status:"error" ~error ()) in
                           Lwt.return current_channel
                       | NoAck -> Lwt.return current_channel)
                   | None ->
                       let* () =
                         send
                           (ack_message ~action_id:"" ~status:"error"
                              ~error:"Invalid mutation frame" ())
                       in
                       let* () = close () in
                       Lwt.return current_channel))
           | None ->
               (match t.handle_mutation with
               | Some handler ->
                   let* result = handler (make_broadcast_fn t) request ~action_id action in
                   (match result with
                   | Ack (Ok ()) ->
                       let* () = send (ack_message ~action_id ~status:"ok" ()) in
                       Lwt.return current_channel
                   | Ack (Error error) ->
                       let* () = send (ack_message ~action_id ~status:"error" ~error ()) in
                       Lwt.return current_channel
                  | NoAck -> Lwt.return current_channel)
              | None ->
                  let* () =
                    send
                      (ack_message ~action_id:"" ~status:"error"
                         ~error:"Invalid mutation frame" ())
                  in
                  let* () = close () in
                  Lwt.return current_channel))
      | _ ->
          let* () =
            send
              (ack_message ~action_id:"" ~status:"error"
                 ~error:"Invalid mutation frame" ())
          in
          let* () = close () in
          Lwt.return current_channel)
  | Some "media" -> (
      match (assoc_json "payload" json, t.handle_media, current_channel) with
      | Some payload, Some handler, Some current ->
          let payload_str = Yojson.Basic.to_string payload in
          let* result = handler (make_broadcast_fn t) request current payload_str in
          (match result with
          | Ok () -> Lwt.return current_channel
          | Error error ->
              let error_msg =
                `Assoc
                  [ ("type", `String "error"); ("message", `String error) ]
                |> json_string
              in
              let* () = send error_msg in
              let* () = close () in
              Lwt.return current_channel)
    | _ ->
        Lwt.return current_channel)
  | _ -> Lwt.return current_channel

let handle_json_message t request websocket current_channel json =
  handle_json_message_with_io t request current_channel json
    ~send:(fun message -> Dream.send websocket message)
    ~close:(fun () -> Dream.close_websocket websocket)
    ~subscribe:(fun channel -> subscribe_websocket t request websocket channel)

let handle_message_with_io t request current_channel message ~send ~close
    ~subscribe =
  incr message_count;
  log_stats t;
  let start = Unix.gettimeofday () in
  Printf.eprintf "[ws] received message: %s\n%!"
    (String.sub message 0 (min 200 (String.length message)));
  let result =
    match message with
    | "ping" ->
        let* () = send pong_message in
        Lwt.return current_channel
    | _ -> (
        let json = try Some (Yojson.Basic.from_string message) with _ -> None in
        match json with
        | Some json ->
            handle_json_message_with_io t request current_channel json ~send ~close
              ~subscribe
        | None ->
            let* () =
              send
                (ack_message ~action_id:"" ~status:"error" ~error:"Unknown message" ())
            in
            Lwt.return current_channel)
  in
  let* final_result = result in
  let elapsed = Unix.gettimeofday () -. start in
  if elapsed > 0.001 then
    Printf.eprintf "[handle_message] elapsed=%.3fs len=%d\n%!" elapsed
      (String.length message);
  Lwt.return final_result

let handle_message t request websocket current_channel message =
  handle_message_with_io t request current_channel message
    ~send:(fun msg -> Dream.send websocket msg)
    ~close:(fun () -> Dream.close_websocket websocket)
    ~subscribe:(fun channel -> subscribe_websocket t request websocket channel)

let rec websocket_handler t request websocket current_channel =
  let receive_start = Unix.gettimeofday () in
  let* message = Dream.receive websocket in
  let receive_time = Unix.gettimeofday () -. receive_start in
  match message with
  | None ->
    Printf.eprintf "[ws] connection closed (receive returned None) after %.3fs wait\n%!" receive_time;
    detach_websocket t websocket
  | Some "" ->
    Printf.eprintf "[ws] received empty message, closing connection\n%!";
    detach_websocket t websocket
  | Some payload ->
    if receive_time < 0.0001 then
      Printf.eprintf "[receive] tight loop! receive_time=%.6fs len=%d\n%!" receive_time (String.length payload);
    let handler_start = Unix.gettimeofday () in
    let* next_channel =
      Lwt.catch
        (fun () -> handle_message t request websocket current_channel payload)
        (fun exn ->
          Printf.eprintf "[ws] handler error: %s\n%!" (Printexc.to_string exn);
          Lwt.return current_channel)
    in
    let handler_time = Unix.gettimeofday () -. handler_start in
    if handler_time > 0.010 then
      Printf.eprintf "[handler] took %.3fs len=%d\n%!" handler_time (String.length payload);
    let* () = Lwt.pause () in
    websocket_handler t request websocket next_channel

let route path t =
  Dream.get path (fun request ->
    Dream.websocket (fun websocket ->
      incr connection_count;
      Printf.eprintf "[ws] new connection, total=%d\n%!" !connection_count;
      Lwt.catch
        (fun () -> websocket_handler t request websocket None)
        (fun exn ->
          Printf.eprintf "[ws] fatal error: %s\n%!" (Printexc.to_string exn);
          Lwt.return_unit)))
