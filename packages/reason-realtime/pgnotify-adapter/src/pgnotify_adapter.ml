open Lwt.Syntax

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
      t.socket := None
  | Some conn ->
      t.conn := None;
      t.socket := None;
      (try conn#finish with _ -> ())

let exec_command_on_conn (conn : Postgresql.connection) query =
  let result = conn#exec query in
  if result#status = Postgresql.Command_ok then
    ()
  else
    failwith result#error

let replay_listens (conn : Postgresql.connection) handlers =
  Hashtbl.iter
    (fun channel channel_handlers ->
      match channel_handlers with
      | [] -> ()
      | _ ->
          exec_command_on_conn conn
            (Printf.sprintf "LISTEN %s" (quote_identifier channel)))
    handlers

let rec connect t =
  Lwt_mutex.with_lock t.mutex (fun () ->
      match !(t.conn) with
      | Some conn when conn#status = Postgresql.Ok -> Lwt.return_unit
      | Some _ ->
          close_connection_locked t;
          connect_fresh_locked t
      | None ->
          connect_fresh_locked t)

and connect_fresh_locked t =
  let host, port, user, password, dbname = parse_uri t.db_uri in
  try
    let conn =
      new Postgresql.connection ~host ~port ~dbname ~user ~password ()
    in
    if conn#status = Postgresql.Ok then (
      try
        let socket = lwt_socket_of_connection conn in
        replay_listens conn t.handlers;
        t.conn := Some conn;
        t.socket := Some socket;
        Lwt.return_unit
      with exn ->
        (try conn#finish with _ -> ());
        Lwt.fail exn)
    else
      Lwt.fail_with conn#error_message
  with Postgresql.Error error ->
    Lwt.fail_with (Postgresql.string_of_error error)

let close_connection t =
  Lwt_mutex.with_lock t.mutex (fun () ->
      close_connection_locked t;
      Lwt.return_unit)

let with_connection t f =
  let* () = connect t in
  Lwt_mutex.with_lock t.mutex (fun () ->
      match !(t.conn) with
      | Some conn -> f conn
      | None -> Lwt.fail_with "PostgreSQL notification connection unavailable")

let wait_for_input t =
  match !(t.socket) with
  | Some _socket -> Lwt_unix.sleep 0.1
  | None ->
      Lwt.catch
        (fun () ->
          let* () = connect t in
          Lwt_unix.sleep empty_read_backoff_seconds)
        (fun exn ->
          Printf.eprintf "[pgnotify] listener reconnect failed: %s\n%!"
            (Printexc.to_string exn);
          Lwt_unix.sleep 1.0)

let run_command t query =
  with_connection t (fun conn ->
      try
        let result = conn#exec query in
        if result#status = Postgresql.Command_ok then
          Lwt.return_unit
        else
          Lwt.fail_with result#error
      with Postgresql.Error error ->
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
  match (Hashtbl.find_opt t.handlers channel, notification_to_message payload) with
  | Some handlers, Some message ->
    Lwt_list.iter_p (fun handler -> handler ?wrap:None message) handlers
  | _ -> Lwt.return_unit

let rec poll t =
  if not !(t.polling) then
    Lwt.return_unit
  else (
    let* () = wait_for_input t in
    if not !(t.polling) then
      Lwt.return_unit
    else
    let* notifications =
      Lwt.catch
        (fun () ->
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
                Lwt.return (drain [])))
        (fun exn ->
          Printf.eprintf "[pgnotify] listener reconnect after input failure: %s\n%!"
            (Printexc.to_string exn);
          let* () = close_connection t in
          let* () = Lwt_unix.sleep empty_read_backoff_seconds in
          Lwt.return [])
    in
    let* () =
      Lwt_list.iter_p
        (fun (channel, payload) -> dispatch t channel payload)
        notifications
    in
    let* () =
      match notifications with
      | [] -> Lwt_unix.sleep empty_read_backoff_seconds
      | _ -> Lwt.return_unit
    in
    poll t)

let start t =
  let* () = connect t in
  if !(t.polling) then
    Lwt.return_unit
  else (
    t.polling := true;
    Lwt.async (fun () -> poll t);
    Lwt.return_unit)

let stop t =
  t.polling := false;
  close_connection t

let subscribe t ~channel ~handler =
  let* () = validate_channel channel in
  let current =
    match Hashtbl.find_opt t.handlers channel with
    | Some handlers -> handlers
    | None -> []
  in
  let first_subscription = current = [] in
  if first_subscription then
    let* () = run_command t (Printf.sprintf "LISTEN %s" (quote_identifier channel)) in
    Hashtbl.replace t.handlers channel [ handler ];
    Lwt.return_unit
  else begin
    Hashtbl.replace t.handlers channel (handler :: current);
    Lwt.return_unit
  end

let unsubscribe t ~channel =
  let* () = validate_channel channel in
  match Hashtbl.find_opt t.handlers channel with
  | None -> Lwt.return_unit
  | Some _ ->
      Hashtbl.remove t.handlers channel;
      (* Avoid synchronous UNLISTEN on websocket close paths. Dispatch is gated
         by the local handler table, so stale LISTEN channels are inert. *)
      Lwt.return_unit
