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
  handle_media : (broadcast_fn -> Dream.request -> string -> string -> unit Lwt.t) option;
  handle_disconnect : (broadcast_fn -> string -> unit Lwt.t) option;
  channels : (string, channel) Hashtbl.t;
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

let create ~adapter ~resolve_subscription ~load_snapshot ?handle_mutation ?handle_media ?handle_disconnect () =
  {
    adapter;
    resolve_subscription;
    load_snapshot;
    handle_mutation;
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

let send_to_channel t channel message =
  let send_start = Unix.gettimeofday () in
  match Hashtbl.find_opt t.channels channel with
  | Some { websocket; _ } ->
    Lwt.catch
    (fun () ->
      let* () = Dream.send websocket message in
      let send_time = Unix.gettimeofday () -. send_start in
      if send_time > 0.005 then
        Printf.eprintf "[send] channel=%s took %.3fs len=%d\n%!" channel send_time (String.length message);
      Lwt.return_unit)
    (fun exn ->
      Printf.eprintf "[ws] send to channel %s failed: %s\n%!" channel (Printexc.to_string exn);
      (* Don't remove here - let detach_websocket handle cleanup *)
      Lwt.return_unit)
  | None -> Lwt.return_unit

let broadcast t channel ?(wrap = wrap_patch) message =
  send_to_channel t channel (wrap message)

let unsubscribe_channel t channel =
  (* Call handle_disconnect BEFORE removing from adapter *)
  let* () = match t.handle_disconnect with
    | Some handler ->
      let broadcast_fn target wrap =
        let wrapped = wrap "" in
        send_to_channel t target wrapped
      in
      handler broadcast_fn channel
    | None -> Lwt.return_unit
  in
  Hashtbl.remove t.channels channel;
  Adapter.unsubscribe t.adapter ~channel

let detach_websocket t channel_id =
  decr connection_count;
  Printf.eprintf "[ws] connection closed, total=%d\n%!" !connection_count;
  match channel_id with
  | Some channel ->
    Printf.eprintf "[ws] detach channel=%s\n%!" channel;
    let* () = unsubscribe_channel t channel in
    Printf.eprintf "[ws] detach complete for channel=%s\n%!" channel;
    Lwt.pause ()
  | None -> Lwt.return_unit

let subscribe_websocket t request websocket channel =
  Printf.eprintf "[ws] subscribe channel=%s\n%!" channel;
  let () = Hashtbl.replace t.channels channel { websocket; current_subscription = Some channel; pending_sends = []; send_in_progress = false } in
  let* snapshot = t.load_snapshot request channel in
  let* () = Adapter.subscribe t.adapter ~channel ~handler:(broadcast t channel) in
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
    let* () = unsubscribe_channel t channel in
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

let handle_json_message t request websocket current_channel json =
  incr message_count;
  log_stats t;
  let msg_type = assoc_string "type" json in
  Printf.eprintf "[ws] message type: %s\n%!" (match msg_type with Some t -> t | None -> "unknown");
  match msg_type with
  | Some "ping" ->
    let* () = Dream.send websocket pong_message in
    Lwt.return current_channel
  | Some "select" -> (
    match assoc_string "subscription" json with
    | Some selection ->
      let* channel = t.resolve_subscription request selection in
      (match channel with
      | Some channel ->
        let* new_channel = subscribe_websocket t request websocket channel in
        (match new_channel with
        | Some ch -> Lwt.return (Some ch)
        | None -> Lwt.return current_channel)
      | None -> Lwt.return current_channel)
    | None -> Lwt.return current_channel)
| Some "mutation" -> (
  match (assoc_string "actionId" json, assoc_json "action" json, t.handle_mutation) with
  | Some action_id, Some action, Some handler ->
    let broadcast_fn target wrap =
      let wrapped = wrap "" in
      send_to_channel t target wrapped
    in
    let* result = handler broadcast_fn request ~action_id action in
    (match result with
    | Ack (Ok ()) ->
      let* () = Dream.send websocket (ack_message ~action_id ~status:"ok" ()) in
      Lwt.return current_channel
    | Ack (Error error) ->
      let* () = Dream.send websocket (ack_message ~action_id ~status:"error" ~error ()) in
      let* () = Dream.close_websocket websocket in
      Lwt.return current_channel
    | NoAck ->
      Lwt.return current_channel)
  | _ ->
    let* () = Dream.send websocket (ack_message ~action_id:"" ~status:"error" ~error:"Invalid mutation frame" ()) in
    let* () = Dream.close_websocket websocket in
    Lwt.return current_channel)
| Some "media" -> (
  match (assoc_json "payload" json, t.handle_media, current_channel) with
  | Some payload, Some handler, Some current ->
    let payload_str = Yojson.Basic.to_string payload in
    let broadcast_fn target wrap =
      let wrapped = wrap "" in
      send_to_channel t target wrapped
    in
    let* () = handler broadcast_fn request current payload_str in
    Lwt.return current_channel
  | _ ->
    Lwt.return current_channel)
| _ -> Lwt.return current_channel

let handle_message t request websocket current_channel message =
  incr message_count;
  log_stats t;
  let start = Unix.gettimeofday () in
  Printf.eprintf "[ws] received message: %s\n%!" (String.sub message 0 (min 200 (String.length message)));
  let result =
    match message with
    | "ping" ->
      let* () = Dream.send websocket pong_message in
      Lwt.return current_channel
    | _ ->
      let json =
        try Some (Yojson.Basic.from_string message) with _ -> None
      in
      match json with
      | Some json -> handle_json_message t request websocket current_channel json
      | None ->
        let* () = Dream.send websocket (ack_message ~action_id:"" ~status:"error" ~error:"Unknown message" ()) in
        Lwt.return current_channel
  in
  let* final_result = result in
  let elapsed = Unix.gettimeofday () -. start in
  if elapsed > 0.001 then
    Printf.eprintf "[handle_message] elapsed=%.3fs len=%d\n%!" elapsed (String.length message);
  Lwt.return final_result

let rec websocket_handler t request websocket current_channel =
  let receive_start = Unix.gettimeofday () in
  let* message = Dream.receive websocket in
  let receive_time = Unix.gettimeofday () -. receive_start in
  match message with
  | None ->
    Printf.eprintf "[ws] connection closed (receive returned None) after %.3fs wait\n%!" receive_time;
    detach_websocket t current_channel
  | Some "" ->
    Printf.eprintf "[ws] received empty message, closing connection\n%!";
    detach_websocket t current_channel
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
