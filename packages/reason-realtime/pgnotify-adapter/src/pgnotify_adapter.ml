open Lwt.Syntax

type notification_payload = {
  table_ : string;
  id : Yojson.Safe.t;
  action : string;
  data : Yojson.Safe.t option;
}

type handler = ?wrap:(string -> string) -> string -> unit Lwt.t

type t = {
  db_uri : string;
  conn : Postgresql.connection option ref;
  handlers : (string, handler list) Hashtbl.t;
  polling : bool ref;
}

let create ~db_uri () =
  { db_uri; conn = ref None; handlers = Hashtbl.create 32; polling = ref false }

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

let connect t =
  match !(t.conn) with
  | Some _ -> Lwt.return_unit
  | None ->
      let host, port, user, password, dbname = parse_uri t.db_uri in
      try
        let conn =
          new Postgresql.connection ~host ~port ~dbname ~user ~password ()
        in
        if conn#status = Postgresql.Ok then (
          t.conn := Some conn;
          Lwt.return_unit)
        else
          Lwt.fail_with conn#error_message
      with Postgresql.Error error ->
        Lwt.fail_with (Postgresql.string_of_error error)

let with_connection t f =
  let* () = connect t in
  match !(t.conn) with
  | Some conn -> f conn
  | None -> Lwt.fail_with "PostgreSQL notification connection unavailable"

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
    Lwt_list.iter_p (fun handler -> handler ?wrap:(Some Fun.id) message) handlers
  | _ -> Lwt.return_unit

let rec poll t =
  if not !(t.polling) then
    Lwt.return_unit
  else
    with_connection t (fun conn ->
        let* () = Lwt_unix.sleep 0.1 in
         (try
            conn#consume_input;
            let rec drain () =
              match conn#notifies with
              | None -> ()
              | Some notification ->
                  Lwt.async (fun () ->
                      dispatch t notification.name notification.extra);
                  drain ()
            in
            drain ()
          with
          | Postgresql.Error _ -> ()
          | Failure _ -> ());
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
  match !(t.conn) with
  | None -> Lwt.return_unit
  | Some conn ->
      t.conn := None;
      conn#finish;
      Lwt.return_unit

let subscribe t ~channel ~handler =
  let* () = validate_channel channel in
  let current =
    match Hashtbl.find_opt t.handlers channel with
    | Some handlers -> handlers
    | None -> []
  in
  let first_subscription = current = [] in
  Hashtbl.replace t.handlers channel (handler :: current);
  if first_subscription then
    run_command t (Printf.sprintf "LISTEN %s" (quote_identifier channel))
  else
    Lwt.return_unit

let unsubscribe t ~channel =
  let* () = validate_channel channel in
  Hashtbl.remove t.handlers channel;
  run_command t (Printf.sprintf "UNLISTEN %s" (quote_identifier channel))
