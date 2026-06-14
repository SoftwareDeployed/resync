[@@@coverage exclude_file]

open Lwt.Syntax
open Mutation_result

(* Diagnostic counters *)
let connection_count = ref 0
let message_count = ref 0
let last_log_time = ref 0.0
let next_connection_id = ref 0

let log fmt =
  Printf.ksprintf
    (fun message ->
      Printf.eprintf "[resync %.6f] %s\n%!" (Unix.gettimeofday ()) message)
    fmt

let allocate_connection_id () =
  incr next_connection_id;
  !next_connection_id

let result_label = function
  | Ack (Ok ()) -> "ack_ok"
  | Ack (Error _) -> "ack_error"
  | Ack_after_commit _ -> "ack_after_commit"
  | NoAck -> "no_ack"

type pending_send = {
  payload : string;
  timestamp : float;
}

type subscriber = {
  connection_id : int;
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
    log "stats connections=%d channels=%d messages=%d"
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

let send_with_timeout ?connection_id websocket message =
  let label =
    match connection_id with
    | Some id -> Printf.sprintf "conn=%d " id
    | None -> ""
  in
  let started = Unix.gettimeofday () in
  log "%ssend.begin len=%d" label (String.length message);
  Lwt.catch
    (fun () ->
      let* outcome =
        Lwt.pick
        [
          (let* () = Dream.send websocket message in
           Lwt.return "sent");
          (let* () = Lwt_unix.sleep 2.0 in
           Lwt.return "timeout");
        ]
      in
      log "%ssend.%s elapsed=%.3f len=%d" label outcome
        (Unix.gettimeofday () -. started) (String.length message);
      Lwt.return_unit)
    (fun exn ->
      log "%ssend.error elapsed=%.3f error=%s" label
        (Unix.gettimeofday () -. started) (Printexc.to_string exn);
      Lwt.return_unit)

let rec drain_subscriber_sends subscriber =
  if subscriber.closed then begin
    log "conn=%d send_queue.closed_drop pending=%d"
      subscriber.connection_id (List.length subscriber.pending_sends);
    subscriber.pending_sends <- [];
    subscriber.send_in_progress <- false;
    Lwt.return_unit
  end else match subscriber.pending_sends with
  | [] ->
      log "conn=%d send_queue.empty" subscriber.connection_id;
      subscriber.send_in_progress <- false;
      Lwt.return_unit
  | pending :: rest ->
      subscriber.pending_sends <- rest;
      log "conn=%d send_queue.drain_one remaining=%d age=%.3f"
        subscriber.connection_id (List.length rest)
        (Unix.gettimeofday () -. pending.timestamp);
      let* () =
        send_with_timeout ~connection_id:subscriber.connection_id
          subscriber.websocket pending.payload
      in
      drain_subscriber_sends subscriber

let enqueue_subscriber_send subscriber payload =
  if subscriber.closed then begin
    log "conn=%d send_queue.enqueue_ignored_closed len=%d"
      subscriber.connection_id (String.length payload);
    Lwt.return_unit
  end else begin
    let pending = { payload; timestamp = Unix.gettimeofday () } in
    let pending_sends =
      if List.length subscriber.pending_sends >= max_pending_sends then
        match subscriber.pending_sends with
        | [] -> [ pending ]
        | dropped :: rest ->
            let age = Unix.gettimeofday () -. dropped.timestamp in
            log "conn=%d send_queue.drop_oldest age=%.3f pending=%d"
              subscriber.connection_id age (List.length subscriber.pending_sends);
            rest @ [ pending ]
      else
        subscriber.pending_sends @ [ pending ]
    in
    subscriber.pending_sends <- pending_sends;
    log "conn=%d send_queue.enqueued len=%d pending=%d draining=%b"
      subscriber.connection_id (String.length payload)
      (List.length subscriber.pending_sends) subscriber.send_in_progress;
    if not subscriber.send_in_progress then (
      subscriber.send_in_progress <- true;
      log "conn=%d send_queue.start_drain" subscriber.connection_id;
      Lwt.async (fun () ->
          let* () = Lwt.pause () in
          drain_subscriber_sends subscriber));
    Lwt.return_unit
  end

let send_websocket ?connection_id t websocket message =
  match find_subscriber_for_websocket t websocket with
  | Some subscriber -> enqueue_subscriber_send subscriber message
  | None -> send_with_timeout ?connection_id websocket message

let make_broadcast_fn t target wrap =
  let wrapped = wrap ~channel:target "" in
  let send_start = Unix.gettimeofday () in
  match Hashtbl.find_opt t.channels target with
  | Some subscribers ->
      log "broadcast.begin channel=%s subscribers=%d len=%d"
        target (List.length subscribers) (String.length wrapped);
      Lwt_list.iter_p
        (fun subscriber ->
          Lwt.catch
            (fun () ->
              let* () = enqueue_subscriber_send subscriber wrapped in
              let send_time = Unix.gettimeofday () -. send_start in
              log "broadcast.enqueue channel=%s conn=%d elapsed=%.3f"
                target subscriber.connection_id send_time;
              Lwt.return_unit)
            (fun exn ->
              log "broadcast.error channel=%s conn=%d error=%s" target
                subscriber.connection_id (Printexc.to_string exn);
              Lwt.return_unit))
        subscribers
  | None ->
      log "broadcast.no_subscribers channel=%s len=%d" target
        (String.length wrapped);
      Lwt.return_unit

let run_mutation_handler t request ~action_id ~mutation_name action db =
  let (module Action_store : Action_store.S) = t.action_store in
  let run_dispatch_or_handler () =
    log "mutation.handler.begin mutation=%s action=%s" mutation_name action_id;
    match t.dispatch_mutation with
    | Some dispatch ->
        log "mutation.dispatch.check mutation=%s action=%s" mutation_name action_id;
        (match dispatch db ~mutation_name action with
         | Some promise ->
             let open Lwt.Syntax in
             log "mutation.dispatch.begin mutation=%s action=%s" mutation_name action_id;
             let* result = promise in
             log "mutation.dispatch.end mutation=%s action=%s result=%s"
               mutation_name action_id
               (match result with Ok () -> "ok" | Error _ -> "error");
             Lwt.return (
               match result with
               | Ok () -> Mutation_result.Ack (Ok ())
               | Error msg -> Mutation_result.Ack (Error msg))
         | None ->
             (match t.handle_mutation with
              | Some handler ->
                  log "mutation.custom_handler.begin mutation=%s action=%s"
                    mutation_name action_id;
                  let* result =
                    handler (make_broadcast_fn t) request ~db ~action_id
                      ~mutation_name action
                  in
                  log "mutation.custom_handler.end mutation=%s action=%s result=%s"
                    mutation_name action_id (result_label result);
                  Lwt.return result
              | None ->
                  log "mutation.invalid.no_handler mutation=%s action=%s"
                    mutation_name action_id;
                  Lwt.return (Mutation_result.Ack (Error "Invalid mutation frame"))))
    | None ->
        (match t.handle_mutation with
         | Some handler ->
             log "mutation.custom_handler.begin mutation=%s action=%s"
               mutation_name action_id;
             let* result =
               handler (make_broadcast_fn t) request ~db ~action_id
                 ~mutation_name action
             in
             log "mutation.custom_handler.end mutation=%s action=%s result=%s"
               mutation_name action_id (result_label result);
             Lwt.return result
         | None ->
             log "mutation.invalid.no_handler mutation=%s action=%s"
               mutation_name action_id;
             Lwt.return (Mutation_result.Ack (Error "Invalid mutation frame")))
  in
  let* result =
    Action_store.with_guard db ~mutation_name ~action_id run_dispatch_or_handler
  in
  log "mutation.guard.end mutation=%s action=%s result=%s"
    mutation_name action_id (result_label result);
  Lwt.return result

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
              | Ack_after_commit _ -> Hashtbl.replace t.in_memory_actions key `Ok
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
  log "mutation.no_db_guard.begin mutation=%s action=%s" mutation_name action_id;
  with_in_memory_action_guard t ~mutation_name ~action_id (fun () ->
    let* result =
      handler (make_broadcast_fn t) request ~action_id ~mutation_name action
    in
    log "mutation.no_db_handler.end mutation=%s action=%s result=%s"
      mutation_name action_id (result_label result);
    Lwt.return result)

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
  log "mutation.exception mutation=%s action=%s error=%s"
    mutation_name action_id msg;
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
         log "mutation.exception_record_failed mutation=%s action=%s error=%s"
           mutation_name action_id (Printexc.to_string record_exn);
         Lwt.return_unit)
  in
  Lwt.return (Ack (Error msg))

let run_mutation_with_guard t request ~action_id ~mutation_name action =
  log "mutation.run.begin mutation=%s action=%s" mutation_name action_id;
  match t.dispatch_mutation, t.handle_mutation, t.handle_mutation_without_db with
  | None, None, Some handler ->
      run_mutation_without_db_handler t request ~action_id ~mutation_name action handler
  | _ ->
      Lwt.catch
        (fun () ->
          log "mutation.use_db.begin mutation=%s action=%s" mutation_name action_id;
          let* result =
            t.use_db request
              (run_mutation_handler t request ~action_id ~mutation_name action)
          in
          log "mutation.use_db.end mutation=%s action=%s result=%s"
            mutation_name action_id (result_label result);
          Lwt.return result)
        (record_exception_and_ack_error t request ~mutation_name ~action_id)

let send_mutation_result ~send ~channel ~action_id current_channels = function
  | Ack (Ok ()) ->
      log "mutation.ack_send.begin action=%s status=ok channel=%s"
        action_id channel;
      let* () = send (ack_message ~channel ~action_id ~status:"ok" ()) in
      log "mutation.ack_send.end action=%s status=ok" action_id;
      Lwt.return current_channels
  | Ack_after_commit after_commit ->
      log "mutation.ack_send.begin action=%s status=ok_after_commit channel=%s"
        action_id channel;
      let* () = send (ack_message ~channel ~action_id ~status:"ok" ()) in
      log "mutation.ack_send.end action=%s status=ok_after_commit" action_id;
      let* () =
        log "mutation.after_commit.begin action=%s" action_id;
        Lwt.catch
          (fun () ->
            let started = Unix.gettimeofday () in
            let* () = after_commit () in
            log "mutation.after_commit.end action=%s elapsed=%.3f"
              action_id (Unix.gettimeofday () -. started);
            Lwt.return_unit)
          (fun exn ->
            log "mutation.after_commit.error action=%s error=%s"
              action_id (Printexc.to_string exn);
            Lwt.return_unit)
      in
      Lwt.return current_channels
  | Ack (Error error) ->
      log "mutation.ack_send.begin action=%s status=error channel=%s error=%s"
        action_id channel error;
      let* () = send (ack_message ~channel ~action_id ~status:"error" ~error ()) in
      log "mutation.ack_send.end action=%s status=error" action_id;
      Lwt.return current_channels
  | NoAck ->
      log "mutation.no_ack action=%s" action_id;
      Lwt.return current_channels

let pong_message = `Assoc [ ("type", `String "pong") ] |> json_string

let close_websocket_safely websocket =
  log "websocket.close.begin";
  Lwt.catch
    (fun () ->
      let started = Unix.gettimeofday () in
      let* outcome =
        Lwt.pick
        [
          (let* () = Dream.close_websocket websocket in
           Lwt.return "closed");
          (let* () = Lwt_unix.sleep 1.0 in
           Lwt.return "timeout");
        ]
      in
      log "websocket.close.%s elapsed=%.3f" outcome
        (Unix.gettimeofday () -. started);
      Lwt.return_unit)
    (fun exn ->
      log "websocket.close.error error=%s" (Printexc.to_string exn);
      Lwt.return_unit)

let send_to_channel t channel message =
  make_broadcast_fn t channel (fun ~channel:_ _ -> message)

let broadcast t channel ?(wrap = wrap_patch) message =
  send_to_channel t channel (wrap ~channel message)

let unsubscribe_channel t channel =
  log "unsubscribe_channel.begin channel=%s" channel;
  let* () = match t.handle_disconnect with
    | Some handler ->
        Lwt.catch
          (fun () ->
            log "unsubscribe_channel.handle_disconnect.begin channel=%s" channel;
            let* () = handler (make_broadcast_fn t) channel in
            log "unsubscribe_channel.handle_disconnect.end channel=%s" channel;
            Lwt.return_unit)
          (fun exn ->
            log "unsubscribe_channel.handle_disconnect.error channel=%s error=%s"
              channel (Printexc.to_string exn);
            Lwt.return_unit)
    | None -> Lwt.return_unit
  in
  let* () =
    Lwt.catch
      (fun () ->
        log "unsubscribe_channel.adapter.begin channel=%s" channel;
        let started = Unix.gettimeofday () in
        let* () = Adapter.unsubscribe t.adapter ~channel in
        log "unsubscribe_channel.adapter.end channel=%s elapsed=%.3f"
          channel (Unix.gettimeofday () -. started);
        Lwt.return_unit)
      (fun exn ->
        log "unsubscribe_channel.adapter.error channel=%s error=%s"
          channel (Printexc.to_string exn);
        Lwt.return_unit)
  in
  log "unsubscribe_channel.end channel=%s" channel;
  Lwt.return_unit

let remove_websocket_from_channel ?connection_id t channel websocket =
  let label =
    match connection_id with
    | Some id -> Printf.sprintf "conn=%d " id
    | None -> ""
  in
  log "%sremove_channel.begin channel=%s" label channel;
  let* should_unsubscribe =
    Lwt_mutex.with_lock t.channels_mutex (fun () ->
      log "%sremove_channel.mutex.enter channel=%s" label channel;
      match Hashtbl.find_opt t.channels channel with
      | None ->
          log "%sremove_channel.not_found channel=%s" label channel;
          Lwt.return_false
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
            (log "%sremove_channel.no_match channel=%s subscribers=%d"
               label channel (List.length subscribers);
             Lwt.return_false)
          else if remaining = [] then begin
            Hashtbl.remove t.channels channel;
            log "%sremove_channel.removed_last channel=%s removed=%d"
              label channel (List.length removed);
            Lwt.return_true
          end
          else begin
            Hashtbl.replace t.channels channel remaining;
            log "%sremove_channel.removed_one channel=%s removed=%d remaining=%d"
              label channel (List.length removed) (List.length remaining);
            Lwt.return_false
          end)
  in
  log "%sremove_channel.mutex.exit channel=%s should_unsubscribe=%b"
    label channel should_unsubscribe;
  if should_unsubscribe then
    unsubscribe_channel t channel
  else begin
    log "%sremove_channel.end channel=%s" label channel;
    Lwt.return_unit
  end

let channels_for_websocket t websocket =
  Hashtbl.fold
    (fun channel subscribers acc ->
      if List.exists (fun subscriber -> subscriber.websocket == websocket) subscribers
      then channel :: acc
      else acc)
    t.channels []

let detach_websocket ?connection_id t websocket =
  let label =
    match connection_id with
    | Some id -> Printf.sprintf "conn=%d " id
    | None -> ""
  in
  decr connection_count;
  log "%sdetach.begin total=%d" label !connection_count;
  let channels = channels_for_websocket t websocket in
  log "%sdetach.channels count=%d channels=%s" label (List.length channels)
    (String.concat "," channels);
  let* () =
    Lwt_list.iter_s
      (fun channel ->
        log "%sdetach.channel.begin channel=%s" label channel;
        let* () = remove_websocket_from_channel ?connection_id t channel websocket in
        log "%sdetach.channel.end channel=%s" label channel;
        Lwt.return_unit)
      channels
  in
  log "%sdetach.end" label;
  Lwt.return_unit

let subscribe_websocket ?(connection_id = 0) t request websocket channel =
  log "conn=%d subscribe.begin channel=%s" connection_id channel;
  Lwt.catch
    (fun () ->
      let snapshot_start = Unix.gettimeofday () in
      log "conn=%d subscribe.snapshot.begin channel=%s" connection_id channel;
      let* snapshot = t.load_snapshot request channel in
      log "conn=%d subscribe.snapshot.end channel=%s elapsed=%.3f len=%d"
        connection_id channel (Unix.gettimeofday () -. snapshot_start)
        (String.length snapshot);
      let* register_result =
        Lwt_mutex.with_lock t.channels_mutex (fun () ->
          log "conn=%d subscribe.mutex.enter channel=%s" connection_id channel;
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
                  connection_id;
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
                  log "conn=%d subscribe.adapter.begin channel=%s"
                    connection_id channel;
                  let adapter_start = Unix.gettimeofday () in
                  let* () = Adapter.subscribe t.adapter ~channel ~handler:(broadcast t channel) in
                  log "conn=%d subscribe.adapter.end channel=%s elapsed=%.3f"
                    connection_id channel (Unix.gettimeofday () -. adapter_start);
                  Lwt.return (Ok subscriber))
                (fun exn ->
                  log "conn=%d subscribe.adapter.error channel=%s error=%s"
                    connection_id channel (Printexc.to_string exn);
                  Lwt.return (Error exn))
            in
            (match subscribe_result with
             | Ok _ as ok ->
                 log "conn=%d subscribe.mutex.exit channel=%s registered=true first=true"
                   connection_id channel;
                 Lwt.return ok
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
                 log "conn=%d subscribe.rollback channel=%s" connection_id channel;
                 Lwt.return (Error exn))
          else
            (log "conn=%d subscribe.mutex.exit channel=%s registered=true first=false"
               connection_id channel;
             Lwt.return (Ok subscriber)))
      in
      match register_result with
      | Error exn ->
          log "conn=%d subscribe.failed channel=%s error=%s"
            connection_id channel (Printexc.to_string exn);
          Lwt.return_none
      | Ok subscriber ->
          let* snapshot_sent =
            Lwt.catch
              (fun () ->
                log "conn=%d subscribe.snapshot_enqueue.begin channel=%s"
                  connection_id channel;
                let* () = enqueue_subscriber_send subscriber (wrap_snapshot ~channel snapshot) in
                log "conn=%d subscribe.snapshot_enqueue.end channel=%s closed=%b"
                  connection_id channel subscriber.closed;
                Lwt.return_true)
              (fun exn ->
                log "conn=%d subscribe.snapshot_enqueue.error channel=%s error=%s"
                  connection_id channel (Printexc.to_string exn);
                Lwt.return_false)
          in
          if snapshot_sent && not subscriber.closed then begin
            log "conn=%d subscribe.end channel=%s result=ok" connection_id channel;
            Lwt.return (Some channel)
          end
          else begin
            log "conn=%d subscribe.cleanup channel=%s snapshot_sent=%b closed=%b"
              connection_id channel snapshot_sent subscriber.closed;
            let* () =
              remove_websocket_from_channel ~connection_id t channel websocket
            in
            Lwt.return_none
          end)
    (fun exn ->
      log "conn=%d subscribe.exception channel=%s error=%s"
        connection_id channel (Printexc.to_string exn);
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

let handle_json_message_with_io ?connection_id t request current_channels json ~send ~close
    ~subscribe ~unsubscribe =
  let label =
    match connection_id with
    | Some id -> Printf.sprintf "conn=%d " id
    | None -> ""
  in
  let msg_type = assoc_string "type" json in
  log "%smessage.type=%s current_channels=%d" label
    (match msg_type with Some t -> t | None -> "unknown")
    (List.length current_channels);
  let get_channel () = match current_channels with ch :: _ -> ch | [] -> "" in
  match msg_type with
  | Some "ping" ->
    log "%sping.begin" label;
    let* () = send pong_message in
    log "%sping.end" label;
    Lwt.return current_channels
  | Some "select" -> (
    match assoc_string "subscription" json with
    | Some selection ->
      log "%sselect.resolve.begin selection=%s" label selection;
      let* channel = t.resolve_subscription request selection in
      log "%sselect.resolve.end selection=%s channel=%s" label selection
        (Option.value channel ~default:"<none>");
      (match channel with
       | Some channel ->
         let* new_channel = subscribe channel in
         (match new_channel with
          | Some ch ->
            let updated = ch :: List.filter (fun c -> c <> ch) current_channels in
            log "%sselect.end channel=%s result=subscribed current_channels=%d"
              label ch (List.length updated);
            Lwt.return updated
          | None ->
            log "%sselect.end selection=%s result=subscribe_none" label selection;
            Lwt.return current_channels)
       | None ->
         log "%sselect.end selection=%s result=unresolved" label selection;
         Lwt.return current_channels)
    | None ->
      log "%sselect.missing_subscription" label;
      Lwt.return current_channels)
  | Some "unsubscribe" -> (
    match assoc_string "channel" json with
    | Some channel ->
      log "%sunsubscribe.begin channel=%s" label channel;
      let* () = unsubscribe channel in
      let updated = List.filter (fun c -> c <> channel) current_channels in
      log "%sunsubscribe.end channel=%s current_channels=%d"
        label channel (List.length updated);
      Lwt.return updated
    | None ->
      log "%sunsubscribe.missing_channel" label;
      Lwt.return current_channels)
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
              log "%smutation.invalid_missing_kind action=%s" label action_id;
              let* () =
                send
                  (ack_message ~channel ~action_id ~status:"error"
                     ~error:"Invalid mutation frame: missing kind" ())
              in
              let* () = close () in
              Lwt.return current_channels
          | Some mutation_name -> (
              log "%smutation.begin mutation=%s action=%s channel=%s"
                label mutation_name action_id channel;
              let run_and_ack () =
                let* result =
                  run_mutation_with_guard t request ~action_id ~mutation_name action
                in
                let* next =
                  send_mutation_result ~send ~channel ~action_id current_channels result
                in
                log "%smutation.end mutation=%s action=%s result=%s"
                  label mutation_name action_id (result_label result);
                Lwt.return next
              in
              match t.validate_mutation with
              | Some validate ->
                  log "%smutation.validate.begin mutation=%s action=%s"
                    label mutation_name action_id;
                  let* validation = validate request action in
                  (match validation with
                  | Error error ->
                      log "%smutation.validate.error mutation=%s action=%s error=%s"
                        label mutation_name action_id error;
                      let* () = send (ack_message ~channel ~action_id ~status:"error" ~error ()) in
                      Lwt.return current_channels
                  | Ok () ->
                      log "%smutation.validate.ok mutation=%s action=%s"
                        label mutation_name action_id;
                      run_and_ack ()
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
          log "%smutation.invalid_frame action=%s" label action_id;
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
          log "%smedia.begin channel=%s len=%d" label current
            (String.length payload_str);
          let* result = handler (make_broadcast_fn t) request current payload_str in
          (match result with
          | Ok () ->
              log "%smedia.end channel=%s result=ok" label current;
              Lwt.return current_channels
          | Error error ->
              log "%smedia.end channel=%s result=error error=%s"
                label current error;
              let error_msg =
                `Assoc
                  [ ("type", `String "error"); ("message", `String error) ]
                |> json_string
              in
              let* () = send error_msg in
              let* () = close () in
              Lwt.return current_channels)
    | _ ->
        log "%smedia.ignored" label;
        Lwt.return current_channels)
  | _ ->
      log "%smessage.ignored_unknown" label;
      Lwt.return current_channels

let handle_json_message t request websocket current_channels json =
  handle_json_message_with_io t request current_channels json
    ~send:(fun message -> send_websocket t websocket message)
    ~close:(fun () -> close_websocket_safely websocket)
    ~subscribe:(fun channel -> subscribe_websocket t request websocket channel)
    ~unsubscribe:(fun channel -> remove_websocket_from_channel t channel websocket)

let handle_message_with_io ?connection_id t request current_channels message ~send ~close
    ~subscribe ~unsubscribe =
  let label =
    match connection_id with
    | Some id -> Printf.sprintf "conn=%d " id
    | None -> ""
  in
  incr message_count;
  log_stats t;
  let start = Unix.gettimeofday () in
  log "%smessage.received len=%d preview=%s" label (String.length message)
    (String.sub message 0 (min 200 (String.length message)));
  let result =
    match message with
    | "ping" ->
        log "%sping_legacy.begin" label;
        let* () = send pong_message in
        log "%sping_legacy.end" label;
        Lwt.return current_channels
    | _ -> (
        let json = try Some (Yojson.Basic.from_string message) with _ -> None in
        match json with
        | Some json ->
            handle_json_message_with_io ?connection_id t request current_channels json ~send ~close
              ~subscribe ~unsubscribe
        | None ->
            log "%smessage.parse_failed len=%d" label (String.length message);
            let* () =
              send
                (ack_message ~channel:(match current_channels with ch :: _ -> ch | [] -> "") ~action_id:"" ~status:"error" ~error:"Unknown message" ())
            in
            Lwt.return current_channels)
  in
  let* final_result = result in
  let elapsed = Unix.gettimeofday () -. start in
  log "%smessage.handled elapsed=%.3f len=%d current_channels=%d"
    label elapsed (String.length message) (List.length final_result);
  Lwt.return final_result

let handle_message t request websocket connection_id current_channels message =
  handle_message_with_io ~connection_id t request current_channels message
    ~send:(fun msg -> send_websocket ~connection_id t websocket msg)
    ~close:(fun () -> close_websocket_safely websocket)
    ~subscribe:(fun channel ->
      subscribe_websocket ~connection_id t request websocket channel)
    ~unsubscribe:(fun channel ->
      remove_websocket_from_channel ~connection_id t channel websocket)

let rec websocket_handler t request websocket connection_id current_channels =
  let receive_start = Unix.gettimeofday () in
  log "conn=%d receive.begin current_channels=%d"
    connection_id (List.length current_channels);
  let* message = Dream.receive websocket in
  let receive_time = Unix.gettimeofday () -. receive_start in
  match message with
  | None ->
    log "conn=%d receive.none elapsed=%.3f" connection_id receive_time;
    detach_websocket ~connection_id t websocket
  | Some "" ->
    log "conn=%d receive.empty elapsed=%.3f" connection_id receive_time;
    let* () = detach_websocket ~connection_id t websocket in
    close_websocket_safely websocket
  | Some payload ->
    if receive_time < 0.0001 then
      log "conn=%d receive.tight elapsed=%.6f len=%d"
        connection_id receive_time (String.length payload);
    log "conn=%d receive.some elapsed=%.3f len=%d"
      connection_id receive_time (String.length payload);
    let handler_start = Unix.gettimeofday () in
    let* next_channels =
      Lwt.catch
        (fun () -> handle_message t request websocket connection_id current_channels payload)
        (fun exn ->
          log "conn=%d handler.error error=%s"
            connection_id (Printexc.to_string exn);
          Lwt.return current_channels)
    in
    let handler_time = Unix.gettimeofday () -. handler_start in
    log "conn=%d handler.end elapsed=%.3f len=%d next_channels=%d"
      connection_id handler_time (String.length payload) (List.length next_channels);
    let* () = Lwt.pause () in
    websocket_handler t request websocket connection_id next_channels

let route path t =
  Dream.get path (fun request ->
    Dream.websocket ~close:false (fun websocket ->
      let connection_id = allocate_connection_id () in
      incr connection_count;
      log "conn=%d open total=%d path=%s" connection_id !connection_count path;
      Lwt.catch
        (fun () -> websocket_handler t request websocket connection_id [])
        (fun exn ->
          log "conn=%d fatal.error error=%s" connection_id
            (Printexc.to_string exn);
          Lwt.return_unit)))
