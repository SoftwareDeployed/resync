open Lwt.Syntax

let log fmt =
  Printf.ksprintf
    (fun message ->
      Printf.eprintf "[pgnotify %.6f] %s\n%!" (Unix.gettimeofday ()) message)
    fmt

type notification_payload = {
  table_ : string;
  id : Yojson.Safe.t;
  action : string;
  data : Yojson.Safe.t option;
}

type handler = ?wrap:(channel:string -> string -> string) -> string -> unit Lwt.t

type t = {
  db_uri : string;
  conn : Postgresql.connection option ref;
  socket : Lwt_unix.file_descr option ref;
  mutex : Lwt_mutex.t;
  handlers : (string, handler list) Hashtbl.t;
  polling : bool ref;
  poll_iterations : int ref;
}

let empty_read_backoff_seconds = 0.01

let create ~db_uri () =
  {
    db_uri;
    conn = ref None;
    socket = ref None;
    mutex = Lwt_mutex.create ();
    handlers = Hashtbl.create 32;
    polling = ref false;
    poll_iterations = ref 0;
  }

let parse_uri uri_str =
  let uri = Uri.of_string uri_str in
  let host = Option.value (Uri.host uri) ~default:"127.0.0.1" in
  let port =
    match Uri.port uri with
    | Some port -> string_of_int port
    | None -> "5432"
  in
  let user, password =
    match Uri.userinfo uri with
    | Some userinfo -> (
        match String.split_on_char ':' userinfo with
        | [ user ] -> user, ""
        | user :: password :: _ -> user, password
        | [] -> "", "")
    | None -> "", ""
  in
  let path = Uri.path uri in
  let dbname =
    if String.length path > 1 && path.[0] = '/' then
      String.sub path 1 (String.length path - 1)
    else
      path
  in
  host, port, user, password, dbname

let file_descr_of_socket socket =
  (Obj.magic socket : Unix.file_descr)

let lwt_socket_of_connection conn =
  conn#set_nonblocking true;
  conn#socket
  |> file_descr_of_socket
  |> Lwt_unix.of_unix_file_descr ~blocking:false ~set_flags:false

let quote_identifier identifier =
  let buffer = Buffer.create (String.length identifier + 2) in
  Buffer.add_char buffer '"';
  String.iter
    (fun char ->
      if char = '"' then
        Buffer.add_string buffer "\"\""
      else
        Buffer.add_char buffer char)
    identifier;
  Buffer.add_char buffer '"';
  Buffer.contents buffer

let close_connection_locked t =
  match !(t.conn) with
  | None ->
      log "connection.close_locked no_connection";
      t.socket := None
  | Some conn ->
      log "connection.close_locked begin";
      t.conn := None;
      t.socket := None;
      (try conn#finish with exn ->
        log "connection.close_locked.finish_error error=%s"
          (Printexc.to_string exn));
      log "connection.close_locked end"

let exec_command_on_conn (conn : Postgresql.connection) query =
  let started = Unix.gettimeofday () in
  log "command.begin query=%s" query;
  conn#set_nonblocking false;
  Fun.protect
    ~finally:(fun () ->
      try
        conn#set_nonblocking true;
        log "command.restore_nonblocking query=%s elapsed=%.3f"
          query (Unix.gettimeofday () -. started)
      with exn ->
        log "command.restore_nonblocking_error query=%s error=%s"
          query (Printexc.to_string exn))
    (fun () ->
      let result = conn#exec query in
      if result#status = Postgresql.Command_ok then begin
        log "command.end query=%s status=ok elapsed=%.3f"
          query (Unix.gettimeofday () -. started);
        ()
      end else begin
        log "command.end query=%s status=error elapsed=%.3f error=%s"
          query (Unix.gettimeofday () -. started) result#error;
        failwith result#error
      end)

let replay_listens (conn : Postgresql.connection) handlers =
  log "replay_listens.begin channels=%d" (Hashtbl.length handlers);
  Hashtbl.iter
    (fun channel channel_handlers ->
      match channel_handlers with
      | [] -> ()
      | _ ->
          exec_command_on_conn conn
            (Printf.sprintf "LISTEN %s" (quote_identifier channel)))
    handlers
  ;
  log "replay_listens.end"

let rec connect t =
  log "connect.begin";
  Lwt_mutex.with_lock t.mutex (fun () ->
      log "connect.mutex.enter";
      match !(t.conn) with
      | Some conn when conn#status = Postgresql.Ok ->
          log "connect.reuse_ok";
          Lwt.return_unit
      | Some _ ->
          log "connect.reconnect_status_not_ok";
          close_connection_locked t;
          connect_fresh_locked t
      | None ->
          log "connect.no_connection";
          connect_fresh_locked t)

and connect_fresh_locked t =
  let host, port, user, password, dbname = parse_uri t.db_uri in
  log "connect_fresh.begin host=%s port=%s dbname=%s" host port dbname;
  try
    let conn =
      new Postgresql.connection ~host ~port ~dbname ~user ~password ()
    in
    if conn#status = Postgresql.Ok then (
      try
        log "connect_fresh.connected";
        let socket = lwt_socket_of_connection conn in
        replay_listens conn t.handlers;
        t.conn := Some conn;
        t.socket := Some socket;
        log "connect_fresh.end status=ok handlers=%d" (Hashtbl.length t.handlers);
        Lwt.return_unit
      with exn ->
        log "connect_fresh.error_after_connect error=%s" (Printexc.to_string exn);
        (try conn#finish with _ -> ());
        Lwt.fail exn)
    else begin
      log "connect_fresh.status_error error=%s" conn#error_message;
      Lwt.fail_with conn#error_message
    end
  with Postgresql.Error error ->
    log "connect_fresh.exception error=%s" (Postgresql.string_of_error error);
    Lwt.fail_with (Postgresql.string_of_error error)

let close_connection t =
  log "connection.close.begin";
  Lwt_mutex.with_lock t.mutex (fun () ->
      close_connection_locked t;
      log "connection.close.end";
      Lwt.return_unit)

let with_connection t f =
  log "with_connection.begin";
  let* () = connect t in
  Lwt_mutex.with_lock t.mutex (fun () ->
      log "with_connection.mutex.enter";
      match !(t.conn) with
      | Some conn ->
          let started = Unix.gettimeofday () in
          let* result = f conn in
          log "with_connection.end elapsed=%.3f"
            (Unix.gettimeofday () -. started);
          Lwt.return result
      | None ->
          log "with_connection.no_connection";
          Lwt.fail_with "PostgreSQL notification connection unavailable")

let wait_for_input t =
  match !(t.socket) with
  | Some socket ->
      log "wait_for_input.begin socket=some";
      Lwt.catch
        (fun () ->
          let started = Unix.gettimeofday () in
          let* outcome =
            Lwt.pick
            [
              (let* () = Lwt_unix.wait_read socket in
               Lwt.return "readable");
              (let* () = Lwt_unix.sleep 1.0 in
               Lwt.return "timeout");
            ]
          in
          log "wait_for_input.%s elapsed=%.3f" outcome
            (Unix.gettimeofday () -. started);
          Lwt.return_unit)
        (fun exn ->
          log "wait_for_input.error error=%s" (Printexc.to_string exn);
          let* () = close_connection t in
          Lwt_unix.sleep empty_read_backoff_seconds)
  | None ->
      log "wait_for_input.begin socket=none";
      Lwt.catch
        (fun () ->
          let* () = connect t in
          log "wait_for_input.connected_after_none";
          Lwt_unix.sleep empty_read_backoff_seconds)
        (fun exn ->
          log "wait_for_input.reconnect_error error=%s" (Printexc.to_string exn);
          Lwt_unix.sleep 1.0)

let run_command t query =
  log "run_command.begin query=%s" query;
  with_connection t (fun conn ->
      try
        exec_command_on_conn conn query;
        log "run_command.end query=%s status=ok" query;
        Lwt.return_unit
      with Postgresql.Error error ->
        log "run_command.error query=%s error=%s"
          query (Postgresql.string_of_error error);
        Lwt.fail_with (Postgresql.string_of_error error))

let validate_channel channel =
  if String.length channel = 0 then
    Lwt.fail_with "Invalid PostgreSQL channel name: empty"
  else if String.length channel > 63 then
    Lwt.fail_with "Invalid PostgreSQL channel name: exceeds maximum length (63)"
  else if String.contains channel '\000' then
    Lwt.fail_with "Invalid PostgreSQL channel name: contains NUL byte"
  else
    Lwt.return_unit

let parse_payload payload =
  let json = Yojson.Safe.from_string payload in
  let open Yojson.Safe.Util in
  let table_ = json |> member "table" |> to_string in
  let id = json |> member "id" in
  let action = json |> member "action" |> to_string in
  let data =
    match json |> member "data" with `Null -> None | value -> Some value
  in
  { table_; id; action; data }

let notification_to_message payload =
  try
    let json = Yojson.Safe.from_string payload in
    let open Yojson.Safe.Util in
    match json |> member "type" with
    | `String "patch" -> Some (Yojson.Safe.to_string json)
    | _ ->
        let payload = parse_payload payload in
        Some
          ((match payload.action with
          | "DELETE" ->
              `Assoc
                [ ("type", `String "patch");
                  ("table", `String payload.table_);
                  ("action", `String "DELETE");
                  ("id", payload.id) ]
          | _ ->
              `Assoc
                [ ("type", `String "patch");
                  ("table", `String payload.table_);
                  ("action", `String payload.action);
                  ( "data",
                    match payload.data with Some data -> data | None -> `Null ) ])
          |> Yojson.Safe.to_string)
  with
  | Yojson.Json_error _ -> None
  | Yojson.Safe.Util.Type_error _ -> None

let dispatch t channel payload =
  log "dispatch.begin channel=%s payload_len=%d" channel (String.length payload);
  match (Hashtbl.find_opt t.handlers channel, notification_to_message payload) with
  | Some handlers, Some message ->
    log "dispatch.handlers channel=%s count=%d message_len=%d"
      channel (List.length handlers) (String.length message);
    let* () = Lwt_list.iter_p (fun handler -> handler ?wrap:None message) handlers in
    log "dispatch.end channel=%s result=delivered" channel;
    Lwt.return_unit
  | Some _, None ->
    log "dispatch.end channel=%s result=payload_parse_failed" channel;
    Lwt.return_unit
  | None, _ ->
    log "dispatch.end channel=%s result=no_handlers" channel;
    Lwt.return_unit

let rec poll t =
  if not !(t.polling) then
    (log "poll.stop";
     Lwt.return_unit)
  else (
    incr t.poll_iterations;
    let iteration = !(t.poll_iterations) in
    if iteration mod 100 = 1 then
      log "poll.iteration.begin iteration=%d handlers=%d"
        iteration (Hashtbl.length t.handlers);
    let* () = wait_for_input t in
    if not !(t.polling) then
      Lwt.return_unit
    else
    let* notifications =
      Lwt.catch
        (fun () ->
          if iteration mod 100 = 1 then
            log "poll.consume.begin iteration=%d" iteration;
          with_connection t (fun conn ->
              conn#consume_input;
              if conn#status <> Postgresql.Ok then
                Lwt.fail_with "PostgreSQL notification connection is not OK"
              else
                let rec drain acc =
                  match conn#notifies with
                  | None -> List.rev acc
                  | Some notification ->
                      drain ((notification.name, notification.extra) :: acc)
                in
                let notifications = drain [] in
                if iteration mod 100 = 1 || notifications <> [] then
                  log "poll.consume.end iteration=%d notifications=%d"
                    iteration (List.length notifications);
                Lwt.return notifications))
        (fun exn ->
          log "poll.consume.error iteration=%d error=%s"
            iteration (Printexc.to_string exn);
          let* () = close_connection t in
          let* () = Lwt_unix.sleep empty_read_backoff_seconds in
          Lwt.return [])
    in
    let* () =
      Lwt_list.iter_p
        (fun (channel, payload) -> dispatch t channel payload)
        notifications
    in
    if iteration mod 100 = 1 || notifications <> [] then
      log "poll.iteration.end iteration=%d notifications=%d"
        iteration (List.length notifications);
    let* () =
      match notifications with
      | [] -> Lwt_unix.sleep empty_read_backoff_seconds
      | _ -> Lwt.return_unit
    in
    poll t)

let start t =
  log "start.begin";
  let* () = connect t in
  if !(t.polling) then
    (log "start.already_polling";
     Lwt.return_unit)
  else (
    t.polling := true;
    log "start.polling_enabled";
    Lwt.async (fun () -> poll t);
    Lwt.return_unit)

let stop t =
  log "stop.begin";
  t.polling := false;
  close_connection t

let subscribe t ~channel ~handler =
  log "subscribe.begin channel=%s" channel;
  let* () = validate_channel channel in
  let current =
    match Hashtbl.find_opt t.handlers channel with
    | Some handlers -> handlers
    | None -> []
  in
  let first_subscription = current = [] in
  log "subscribe.state channel=%s first=%b existing_handlers=%d"
    channel first_subscription (List.length current);
  if first_subscription then
    let* () = run_command t (Printf.sprintf "LISTEN %s" (quote_identifier channel)) in
    Hashtbl.replace t.handlers channel [ handler ];
    log "subscribe.end channel=%s handlers=1 listened=true" channel;
    Lwt.return_unit
  else begin
    Hashtbl.replace t.handlers channel (handler :: current);
    log "subscribe.end channel=%s handlers=%d listened=false"
      channel (List.length current + 1);
    Lwt.return_unit
  end

let unsubscribe t ~channel =
  log "unsubscribe.begin channel=%s" channel;
  let* () = validate_channel channel in
  match Hashtbl.find_opt t.handlers channel with
  | None ->
      log "unsubscribe.end channel=%s result=not_found" channel;
      Lwt.return_unit
  | Some _ ->
      Hashtbl.remove t.handlers channel;
      (* Avoid synchronous UNLISTEN on websocket close paths. Dispatch is gated
         by the local handler table, so stale LISTEN channels are inert. *)
      log "unsubscribe.end channel=%s result=removed" channel;
      Lwt.return_unit
