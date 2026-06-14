[@@@coverage exclude_file]

open Lwt.Syntax
open Mutation_result

(* Diagnostic counters *)
let connection_count = ref 0
let message_count = ref 0
let last_log_time = ref 0.0

type pending_send = {
  payload : string;
  timestamp : float;
}

type subscriber = {
  websocket : Dream.websocket;
  mutable current_subscriptions : string list;
  mutable pending_sends : pending_send list;
  mutable send_in_progress : bool;
  mutable closed : bool;
}

let max_pending_sends = 50

type broadcast_fn = string -> (channel:string -> string -> string) -> unit Lwt.t

type handle_mutation =
  broadcast_fn -> Dream.request -> db:(module Caqti_lwt.CONNECTION) -> action_id:string -> mutation_name:string -> Yojson.Basic.t -> Mutation_result.t Lwt.t

type handle_mutation_without_db =
  broadcast_fn -> Dream.request -> action_id:string -> mutation_name:string -> Yojson.Basic.t -> Mutation_result.t Lwt.t

type dispatch_mutation =
  (module Caqti_lwt.CONNECTION) -> mutation_name:string -> Yojson.Basic.t -> (unit, string) result Lwt.t option

type in_memory_action_status =
  [ `Ok
  | `Failed of string
  | `InProgress of Mutation_result.t Lwt.t
  ]

type t = {
  adapter : Adapter.packed;
  resolve_subscription : Dream.request -> string -> string option Lwt.t;
  load_snapshot : Dream.request -> string -> string Lwt.t;
  action_store : (module Action_store.S);
  use_db : Dream.request -> ((module Caqti_lwt.CONNECTION) -> Mutation_result.t Lwt.t) -> Mutation_result.t Lwt.t;
  handle_mutation : handle_mutation option;
  handle_mutation_without_db : handle_mutation_without_db option;
  dispatch_mutation : dispatch_mutation option;
  validate_mutation : (Dream.request -> Yojson.Basic.t -> (unit, string) result Lwt.t) option;
  handle_media : (broadcast_fn -> Dream.request -> string -> string -> (unit, string) result Lwt.t) option;
  handle_disconnect : (broadcast_fn -> string -> unit Lwt.t) option;
  channels : (string, subscriber list) Hashtbl.t;
  channels_mutex : Lwt_mutex.t;
  in_memory_actions : (string, in_memory_action_status) Hashtbl.t;
}

let create ~adapter ~resolve_subscription ~load_snapshot ?handle_mutation ?handle_mutation_without_db ?dispatch_mutation ?validate_mutation ?handle_media ?handle_disconnect ?(action_store = (module Sql_action_store : Action_store.S)) ?(use_db = Dream.sql) () =
  {
    adapter;
    resolve_subscription;
    load_snapshot;
    action_store;
    use_db;
    handle_mutation;
    handle_mutation_without_db;
    dispatch_mutation;
    validate_mutation;
    handle_media;
    handle_disconnect;
    channels = Hashtbl.create 32;
    channels_mutex = Lwt_mutex.create ();
    in_memory_actions = Hashtbl.create 128;
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

let find_subscriber_for_websocket t websocket =
  Hashtbl.fold
    (fun _channel subscribers found ->
      match found with
      | Some _ -> found
      | None ->
          List.find_opt
            (fun subscriber -> subscriber.websocket == websocket)
            subscribers)
    t.channels None

let send_with_timeout websocket message =
  Lwt.catch
    (fun () ->
      Lwt.pick
        [
          Dream.send websocket message;
          (let* () = Lwt_unix.sleep 2.0 in
           Lwt.return_unit);
        ])
    (fun exn ->
      Printf.eprintf "[ws] send failed: %s\n%!" (Printexc.to_string exn);
      Lwt.return_unit)

let rec drain_subscriber_sends subscriber =
  if subscriber.closed then begin
    subscriber.pending_sends <- [];
    subscriber.send_in_progress <- false;
    Lwt.return_unit
  end else match subscriber.pending_sends with
  | [] ->
      subscriber.send_in_progress <- false;
      Lwt.return_unit
  | pending :: rest ->
      subscriber.pending_sends <- rest;
      let* () = send_with_timeout subscriber.websocket pending.payload in
      drain_subscriber_sends subscriber

let enqueue_subscriber_send subscriber payload =
  if subscriber.closed then
    Lwt.return_unit
  else begin
    let pending = { payload; timestamp = Unix.gettimeofday () } in
    let pending_sends =
      if List.length subscriber.pending_sends >= max_pending_sends then
        match subscriber.pending_sends with
        | [] -> [ pending ]
        | dropped :: rest ->
            let age = Unix.gettimeofday () -. dropped.timestamp in
            Printf.eprintf "[ws] dropping queued send after %.3fs\n%!" age;
            rest @ [ pending ]
      else
        subscriber.pending_sends @ [ pending ]
    in
    subscriber.pending_sends <- pending_sends;
    if not subscriber.send_in_progress then (
      subscriber.send_in_progress <- true;
      Lwt.async (fun () ->
          let* () = Lwt.pause () in
          drain_subscriber_sends subscriber));
    Lwt.return_unit
  end

let send_websocket t websocket message =
  match find_subscriber_for_websocket t websocket with
  | Some subscriber -> enqueue_subscriber_send subscriber message
  | None -> send_with_timeout websocket message

let make_broadcast_fn t target wrap =
  let wrapped = wrap ~channel:target "" in
  let send_start = Unix.gettimeofday () in
  match Hashtbl.find_opt t.channels target with
  | Some subscribers ->
      Lwt_list.iter_p
        (fun subscriber ->
          Lwt.catch
            (fun () ->
              let* () = enqueue_subscriber_send subscriber wrapped in
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

let run_mutation_handler t request ~action_id ~mutation_name action db =
  let (module Action_store : Action_store.S) = t.action_store in
  let run_dispatch_or_handler () =
    match t.dispatch_mutation with
    | Some dispatch ->
        (match dispatch db ~mutation_name action with
         | Some promise ->
             let open Lwt.Syntax in
             let* result = promise in
             Lwt.return (
               match result with
               | Ok () -> Mutation_result.Ack (Ok ())
               | Error msg -> Mutation_result.Ack (Error msg))
         | None ->
             (match t.handle_mutation with
              | Some handler ->
                  handler (make_broadcast_fn t) request ~db ~action_id ~mutation_name action
              | None ->
                  Lwt.return (Mutation_result.Ack (Error "Invalid mutation frame"))))
    | None ->
        (match t.handle_mutation with
         | Some handler ->
             handler (make_broadcast_fn t) request ~db ~action_id ~mutation_name action
         | None ->
             Lwt.return (Mutation_result.Ack (Error "Invalid mutation frame")))
  in
  Action_store.with_guard db ~mutation_name ~action_id run_dispatch_or_handler

let in_memory_action_key ~mutation_name ~action_id =
  mutation_name ^ ":" ^ action_id

let with_in_memory_action_guard t ~mutation_name ~action_id callback =
  let key = in_memory_action_key ~mutation_name ~action_id in
  match Hashtbl.find_opt t.in_memory_actions key with
  | Some `Ok -> Lwt.return (Ack (Ok ()))
  | Some (`Failed msg) -> Lwt.return (Ack (Error msg))
  | Some (`InProgress promise) -> promise
  | None ->
      let promise, wake = Lwt.wait () in
      Hashtbl.replace t.in_memory_actions key (`InProgress promise);
      Lwt.async (fun () ->
        Lwt.catch
          (fun () ->
             let* result = callback () in
             (match result with
              | Ack (Ok ()) -> Hashtbl.replace t.in_memory_actions key `Ok
              | Ack (Error msg) -> Hashtbl.replace t.in_memory_actions key (`Failed msg)
              | NoAck -> Hashtbl.remove t.in_memory_actions key);
             Lwt.wakeup_later wake result;
             Lwt.return_unit)
          (fun exn ->
             let msg = Printexc.to_string exn in
             let result = Ack (Error msg) in
             Hashtbl.replace t.in_memory_actions key (`Failed msg);
             Lwt.wakeup_later wake result;
             Lwt.return_unit));
      promise

let run_mutation_without_db_handler t request ~action_id ~mutation_name action handler =
  with_in_memory_action_guard t ~mutation_name ~action_id (fun () ->
    handler (make_broadcast_fn t) request ~action_id ~mutation_name action)

let json_string value = Yojson.Basic.to_string value

let wrap_patch ~channel message =
  let payload =
    try Yojson.Basic.from_string message with _ -> `String message
  in
  `Assoc
    [
      ("type", `String "patch");
      ("channel", `String channel);
      ("timestamp", `Float (Unix.gettimeofday () *. 1000.0));
      ("payload", payload);
    ]
  |> json_string

let wrap_snapshot ~channel message =
  let payload =
    try Yojson.Basic.from_string message with _ -> `String message
  in
  `Assoc [ ("type", `String "snapshot"); ("channel", `String channel); ("payload", payload) ] |> json_string

let ack_message ~channel ~action_id ~status ?error () =
  let fields = [ ("type", `String "ack"); ("channel", `String channel); ("actionId", `String action_id); ("status", `String status) ] in
  let fields =
    match error with
    | Some error -> ("error", `String error) :: fields
    | None -> fields
  in
  `Assoc fields |> json_string

let record_exception_and_ack_error t request ~mutation_name ~action_id exn =
  let msg = Printexc.to_string exn in
  let* () =
    Lwt.catch
      (fun () ->
         let* _ =
           t.use_db request (fun db ->
             let (module Action_store : Action_store.S) = t.action_store in
             let* _ = Action_store.record_failed db ~mutation_name ~action_id ~msg in
             Lwt.return (Ack (Ok ())))
         in
         Lwt.return_unit)
      (fun record_exn ->
         Printf.eprintf
           "[ws] failed to record mutation exception for %s.%s: %s\n%!"
           mutation_name action_id (Printexc.to_string record_exn);
         Lwt.return_unit)
  in
  Lwt.return (Ack (Error msg))

let run_mutation_with_guard t request ~action_id ~mutation_name action =
  match t.dispatch_mutation, t.handle_mutation, t.handle_mutation_without_db with
  | None, None, Some handler ->
      run_mutation_without_db_handler t request ~action_id ~mutation_name action handler
  | _ ->
      Lwt.catch
        (fun () -> t.use_db request (run_mutation_handler t request ~action_id ~mutation_name action))
        (record_exception_and_ack_error t request ~mutation_name ~action_id)

let send_mutation_result ~send ~channel ~action_id current_channels = function
  | Ack (Ok ()) ->
      let* () = send (ack_message ~channel ~action_id ~status:"ok" ()) in
      Lwt.return current_channels
  | Ack (Error error) ->
      let* () = send (ack_message ~channel ~action_id ~status:"error" ~error ()) in
      Lwt.return current_channels
  | NoAck -> Lwt.return current_channels

let pong_message = `Assoc [ ("type", `String "pong") ] |> json_string

let close_websocket_safely websocket =
  Lwt.catch
    (fun () ->
      Lwt.pick
        [
          Dream.close_websocket websocket;
          (let* () = Lwt_unix.sleep 1.0 in
           Lwt.return_unit);
        ])
    (fun exn ->
      Printf.eprintf "[ws] close failed: %s\n%!" (Printexc.to_string exn);
      Lwt.return_unit)

let send_to_channel t channel message =
  make_broadcast_fn t channel (fun ~channel:_ _ -> message)

let broadcast t channel ?(wrap = wrap_patch) message =
  send_to_channel t channel (wrap ~channel message)

let unsubscribe_channel t channel =
  let* () = match t.handle_disconnect with
    | Some handler ->
        Lwt.catch
          (fun () -> handler (make_broadcast_fn t) channel)
          (fun exn ->
            Printf.eprintf "[ws] handle_disconnect failed for %s: %s\n%!"
              channel (Printexc.to_string exn);
            Lwt.return_unit)
    | None -> Lwt.return_unit
  in
  Lwt.catch
    (fun () -> Adapter.unsubscribe t.adapter ~channel)
    (fun exn ->
      Printf.eprintf "[ws] adapter unsubscribe failed for %s: %s\n%!"
        channel (Printexc.to_string exn);
      Lwt.return_unit)

let remove_websocket_from_channel t channel websocket =
  let* should_unsubscribe =
    Lwt_mutex.with_lock t.channels_mutex (fun () ->
      match Hashtbl.find_opt t.channels channel with
      | None -> Lwt.return_false
      | Some subscribers ->
          let removed =
            subscribers
            |> List.filter (fun subscriber -> subscriber.websocket == websocket)
          in
          let remaining =
            subscribers
            |> List.filter (fun subscriber -> subscriber.websocket != websocket)
          in
          removed
          |> List.iter (fun subscriber ->
               subscriber.current_subscriptions <- List.filter (fun c -> c <> channel) subscriber.current_subscriptions;
               if subscriber.current_subscriptions = [] then begin
                 subscriber.closed <- true;
                 subscriber.pending_sends <- [];
                 subscriber.send_in_progress <- false
               end);
          if List.length remaining = List.length subscribers then
            Lwt.return_false
          else if remaining = [] then begin
            Hashtbl.remove t.channels channel;
            Lwt.return_true
          end
          else begin
            Hashtbl.replace t.channels channel remaining;
            Lwt.return_false
          end)
  in
  if should_unsubscribe then
    unsubscribe_channel t channel
  else
    Lwt.return_unit

let channels_for_websocket t websocket =
  Hashtbl.fold
    (fun channel subscribers acc ->
      if List.exists (fun subscriber -> subscriber.websocket == websocket) subscribers
      then channel :: acc
      else acc)
    t.channels []

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
  Lwt.return_unit

let subscribe_websocket t request websocket channel =
  Printf.eprintf "[ws] subscribe channel=%s\n%!" channel;
  Lwt.catch
    (fun () ->
      let* snapshot = t.load_snapshot request channel in
      let* register_result =
        Lwt_mutex.with_lock t.channels_mutex (fun () ->
          let existing_subscribers =
            match Hashtbl.find_opt t.channels channel with
            | Some subscribers -> subscribers
            | None -> []
          in
          let existing_websocket_subscriber =
            find_subscriber_for_websocket t websocket
          in
          let already_subscribed =
            List.exists (fun subscriber -> subscriber.websocket == websocket) existing_subscribers
          in
          let subscriber =
            match existing_websocket_subscriber with
            | Some subscriber ->
                subscriber.closed <- false;
                subscriber
            | None ->
                {
                  websocket;
                  current_subscriptions = [];
                  pending_sends = [];
                  send_in_progress = false;
                  closed = false;
                }
          in
          if not (List.mem channel subscriber.current_subscriptions) then
            subscriber.current_subscriptions <- channel :: subscriber.current_subscriptions;
          let first_subscription = existing_subscribers = [] in
          let subscribers =
            if already_subscribed then
              existing_subscribers
            else
              subscriber :: existing_subscribers
          in
          Hashtbl.replace t.channels channel subscribers;
          if first_subscription then
            let* subscribe_result =
              Lwt.catch
                (fun () ->
                  let* () = Adapter.subscribe t.adapter ~channel ~handler:(broadcast t channel) in
                  Lwt.return (Ok subscriber))
                (fun exn -> Lwt.return (Error exn))
            in
            (match subscribe_result with
             | Ok _ as ok -> Lwt.return ok
             | Error exn ->
                 let current =
                   match Hashtbl.find_opt t.channels channel with
                   | Some subscribers -> subscribers
                   | None -> []
                 in
                 let remaining =
                   current |> List.filter (fun item -> item.websocket != websocket)
                 in
                 if remaining = [] then
                   Hashtbl.remove t.channels channel
                 else
                   Hashtbl.replace t.channels channel remaining;
                 subscriber.current_subscriptions <-
                   List.filter (fun c -> c <> channel) subscriber.current_subscriptions;
                 if subscriber.current_subscriptions = [] then begin
                   subscriber.closed <- true;
                   subscriber.pending_sends <- [];
                   subscriber.send_in_progress <- false
                 end;
                 Lwt.return (Error exn))
          else
            Lwt.return (Ok subscriber))
      in
      match register_result with
      | Error exn ->
          Printf.eprintf "[ws] failed to subscribe channel %s: %s\n%!"
            channel (Printexc.to_string exn);
          Lwt.return_none
      | Ok subscriber ->
          let* snapshot_sent =
            Lwt.catch
              (fun () ->
                let* () = enqueue_subscriber_send subscriber (wrap_snapshot ~channel snapshot) in
                Lwt.return_true)
              (fun exn ->
                Printf.eprintf "[ws] failed to send snapshot: %s\n%!" (Printexc.to_string exn);
                Lwt.return_false)
          in
          if snapshot_sent && not subscriber.closed then
            Lwt.return (Some channel)
          else begin
            let* () = remove_websocket_from_channel t channel websocket in
            Lwt.return_none
          end)
    (fun exn ->
      Printf.eprintf "[ws] snapshot failed for channel %s: %s\n%!"
        channel (Printexc.to_string exn);
      Lwt.return_none)

let assoc_string key = function
  | `Assoc fields -> (
      match List.assoc_opt key fields with
      | Some (`String value) -> Some value
      | _ -> None)
  | _ -> None

let assoc_json key = function
  | `Assoc fields -> List.assoc_opt key fields
  | _ -> None

let handle_json_message_with_io t request current_channels json ~send ~close
    ~subscribe ~unsubscribe =
  let msg_type = assoc_string "type" json in
  Printf.eprintf "[ws] message type: %s\n%!" (match msg_type with Some t -> t | None -> "unknown");
  let get_channel () = match current_channels with ch :: _ -> ch | [] -> "" in
  match msg_type with
  | Some "ping" ->
    let* () = send pong_message in
    Lwt.return current_channels
  | Some "select" -> (
    match assoc_string "subscription" json with
    | Some selection ->
      let* channel = t.resolve_subscription request selection in
      (match channel with
       | Some channel ->
         let* new_channel = subscribe channel in
         (match new_channel with
          | Some ch ->
            let updated = ch :: List.filter (fun c -> c <> ch) current_channels in
            Lwt.return updated
          | None -> Lwt.return current_channels)
       | None -> Lwt.return current_channels)
    | None -> Lwt.return current_channels)
  | Some "unsubscribe" -> (
    match assoc_string "channel" json with
    | Some channel ->
      let* () = unsubscribe channel in
      Lwt.return (List.filter (fun c -> c <> channel) current_channels)
    | None -> Lwt.return current_channels)
  | Some "mutation" -> (
      match (assoc_string "actionId" json, assoc_json "action" json) with
      | Some action_id, Some action -> (
          let mutation_name =
            match action with
            | `Assoc fields -> (
                match List.assoc_opt "kind" fields with
                | Some (`String kind) -> Some kind
                | _ -> None)
            | _ -> None
          in
          let channel = get_channel () in
          match mutation_name with
          | None ->
              let* () =
                send
                  (ack_message ~channel ~action_id ~status:"error"
                     ~error:"Invalid mutation frame: missing kind" ())
              in
              let* () = close () in
              Lwt.return current_channels
          | Some mutation_name -> (
              let run_and_ack () =
                let* result =
                  run_mutation_with_guard t request ~action_id ~mutation_name action
                in
                send_mutation_result ~send ~channel ~action_id current_channels result
              in
              match t.validate_mutation with
              | Some validate ->
                  let* validation = validate request action in
                  (match validation with
                  | Error error ->
                      let* () = send (ack_message ~channel ~action_id ~status:"error" ~error ()) in
                      Lwt.return current_channels
                  | Ok () -> run_and_ack ()
                  )
               | None -> run_and_ack ()
                 )
         )
      | _ ->
          let action_id =
            match assoc_string "actionId" json with
            | Some action_id -> action_id
            | None -> ""
          in
          let* () =
            send
              (ack_message ~channel:(get_channel ()) ~action_id ~status:"error"
                 ~error:"Invalid mutation frame" ())
          in
          let* () = close () in
          Lwt.return current_channels
  )
  | Some "media" -> (
      match (assoc_json "payload" json, t.handle_media, current_channels) with
      | Some payload, Some handler, current :: _ ->
          let payload_str = Yojson.Basic.to_string payload in
          let* result = handler (make_broadcast_fn t) request current payload_str in
          (match result with
          | Ok () -> Lwt.return current_channels
          | Error error ->
              let error_msg =
                `Assoc
                  [ ("type", `String "error"); ("message", `String error) ]
                |> json_string
              in
              let* () = send error_msg in
              let* () = close () in
              Lwt.return current_channels)
    | _ ->
        Lwt.return current_channels)
  | _ -> Lwt.return current_channels

let handle_json_message t request websocket current_channels json =
  handle_json_message_with_io t request current_channels json
    ~send:(fun message -> send_websocket t websocket message)
    ~close:(fun () -> Dream.close_websocket websocket)
    ~subscribe:(fun channel -> subscribe_websocket t request websocket channel)
    ~unsubscribe:(fun channel -> remove_websocket_from_channel t channel websocket)

let handle_message_with_io t request current_channels message ~send ~close
    ~subscribe ~unsubscribe =
  incr message_count;
  log_stats t;
  let start = Unix.gettimeofday () in
  Printf.eprintf "[ws] received message: %s\n%!"
    (String.sub message 0 (min 200 (String.length message)));
  let result =
    match message with
    | "ping" ->
        let* () = send pong_message in
        Lwt.return current_channels
    | _ -> (
        let json = try Some (Yojson.Basic.from_string message) with _ -> None in
        match json with
        | Some json ->
            handle_json_message_with_io t request current_channels json ~send ~close
              ~subscribe ~unsubscribe
        | None ->
            let* () =
              send
                (ack_message ~channel:(match current_channels with ch :: _ -> ch | [] -> "") ~action_id:"" ~status:"error" ~error:"Unknown message" ())
            in
            Lwt.return current_channels)
  in
  let* final_result = result in
  let elapsed = Unix.gettimeofday () -. start in
  if elapsed > 0.001 then
    Printf.eprintf "[handle_message] elapsed=%.3fs len=%d\n%!" elapsed
      (String.length message);
  Lwt.return final_result

let handle_message t request websocket current_channels message =
  handle_message_with_io t request current_channels message
    ~send:(fun msg -> send_websocket t websocket msg)
    ~close:(fun () -> Dream.close_websocket websocket)
    ~subscribe:(fun channel -> subscribe_websocket t request websocket channel)
    ~unsubscribe:(fun channel -> remove_websocket_from_channel t channel websocket)

let rec websocket_handler t request websocket current_channels =
  let receive_start = Unix.gettimeofday () in
  let* message = Dream.receive websocket in
  let receive_time = Unix.gettimeofday () -. receive_start in
  match message with
  | None ->
    Printf.eprintf "[ws] connection closed (receive returned None) after %.3fs wait\n%!" receive_time;
    detach_websocket t websocket
  | Some "" ->
    Printf.eprintf "[ws] received empty message, closing connection\n%!";
    let* () = detach_websocket t websocket in
    close_websocket_safely websocket
  | Some payload ->
    if receive_time < 0.0001 then
      Printf.eprintf "[receive] tight loop! receive_time=%.6fs len=%d\n%!" receive_time (String.length payload);
    let handler_start = Unix.gettimeofday () in
    let* next_channels =
      Lwt.catch
        (fun () -> handle_message t request websocket current_channels payload)
        (fun exn ->
          Printf.eprintf "[ws] handler error: %s\n%!" (Printexc.to_string exn);
          Lwt.return current_channels)
    in
    let handler_time = Unix.gettimeofday () -. handler_start in
    if handler_time > 0.010 then
      Printf.eprintf "[handler] took %.3fs len=%d\n%!" handler_time (String.length payload);
    let* () = Lwt.pause () in
    websocket_handler t request websocket next_channels

let route path t =
  Dream.get path (fun request ->
    Dream.websocket (fun websocket ->
      incr connection_count;
      Printf.eprintf "[ws] new connection, total=%d\n%!" !connection_count;
      Lwt.catch
        (fun () -> websocket_handler t request websocket [])
        (fun exn ->
          Printf.eprintf "[ws] fatal error: %s\n%!" (Printexc.to_string exn);
          Lwt.return_unit)))
