open Lwt.Syntax

let doc_root =
  match Sys.getenv_opt "TODO_MP_DOC_ROOT" with
  | Some doc_root -> doc_root
  | None -> failwith "TODO_MP_DOC_ROOT is required"

let db_uri =
  match Sys.getenv_opt "DB_URL" with
  | Some uri -> uri
  | None -> failwith "DB_URL is required"

let server_interface =
  match Sys.getenv_opt "SERVER_INTERFACE" with
  | Some interface -> interface
  | None -> "127.0.0.1"

let server_port =
  match Sys.getenv_opt "SERVER_PORT" with
  | Some port -> int_of_string(port)
  | None -> 8898

let get_config request list_id =
  let* list_info = Dream.sql request (Database.Todo.get_list_info list_id) in
  let* todos = Dream.sql request (Database.Todo.get_list list_id) in
  let config : Model.t = { todos; list = list_info } in
  Lwt.return config

let get_config_json request list_id =
  let* config = get_config request list_id in
  Lwt.return (TodoStore.serializeSnapshot config)

let resolve_subscription request selection =
  match RealtimeSubscription.decode_channel selection with
  | None -> Lwt.return_none
  | Some list_id ->
    let* list_info =
      Dream.sql request (Database.Todo.get_list_info list_id)
    in
    Lwt.return (Option.map (fun _ -> list_id) list_info)

let realtime_adapter =
  Adapter.pack
    (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
    (Pgnotify_adapter.create ~db_uri ())

let assoc key = function
  | `Assoc fields -> List.assoc_opt key fields
  | _ -> None

let required_string key json =
  match assoc key json with
  | Some (`String value) -> Ok value
  | _ -> Error ("Missing string field: " ^ key)

let required_bool key json =
  match assoc key json with
  | Some (`Bool value) -> Ok value
  | _ -> Error ("Missing bool field: " ^ key)

let handle_mutation request ~action_id action =
  let kind =
    match assoc "kind" action with
    | Some (`String value) -> Ok value
    | _ -> Error "Missing action kind"
  in
  match kind with
  | Error error -> Lwt.return (Error error)
  | Ok "add_todo" -> (
      match assoc "payload" action with
      | Some payload -> (
          match
            ( required_string "id" payload,
              required_string "list_id" payload,
              required_string "text" payload )
          with
          | Ok id, Ok list_id, Ok text ->
              let* () = Dream.sql request (Database.Todo.add_todo (action_id, id, list_id, text)) in
              Lwt.return (Ok ())
          | Error error, _, _ | _, Error error, _ | _, _, Error error ->
              Lwt.return (Error error))
      | None -> Lwt.return (Error "Missing add_todo payload"))
  | Ok "set_todo_completed" -> (
      match assoc "payload" action with
      | Some payload -> (
          match (required_string "id" payload, required_bool "completed" payload) with
          | Ok id, Ok completed ->
              let* () = Dream.sql request (Database.Todo.set_todo_completed (action_id, id, completed)) in
              Lwt.return (Ok ())
          | Error error, _ | _, Error error -> Lwt.return (Error error))
      | None -> Lwt.return (Error "Missing set_todo_completed payload"))
  | Ok "remove_todo" -> (
      match assoc "payload" action with
      | Some payload -> (
          match required_string "id" payload with
          | Ok id ->
              let* () = Dream.sql request (Database.Todo.remove_todo (action_id, id)) in
              Lwt.return (Ok ())
          | Error error -> Lwt.return (Error error))
      | None -> Lwt.return (Error "Missing remove_todo payload"))
  | Ok _ -> Lwt.return (Error "Unknown action kind")

let realtime_middleware =
  Middleware.create ~adapter:realtime_adapter ~resolve_subscription
    ~load_snapshot:get_config_json ~handle_mutation ()

let generate_uuid () =
  let random_hex len =
    let buf = Buffer.create len in
    for _ = 1 to len do
      let n = Random.int 16 in
      let chars = "0123456789abcdef" in
      Buffer.add_char buf chars.[n]
    done;
    Buffer.contents buf
  in
  let hex8 = random_hex 8 in
  let hex4a = random_hex 4 in
  let hex4b = random_hex 4 in
  let hex4c = random_hex 4 in
  let hex12 = random_hex 12 in
  Printf.sprintf "%s-%s-%s-%s-%s" hex8 hex4a hex4b hex4c hex12

let () =
  Random.self_init ();
  (match Lwt_main.run (Adapter.start realtime_adapter) with
  | () -> ()
  | exception Failure msg ->
    Printf.eprintf "Failed to connect notification listener: %s\n" msg);
  Dream.run ~interface:server_interface ~port:server_port @@ Dream.logger
  @@ Dream.sql_pool ~size:10 db_uri
  @@ Dream.router
       [
         Middleware.route "_events" realtime_middleware;
         Dream.get "/static/**" (Dream.static doc_root);
         Dream.get "/app.js" (fun req ->
           Dream.from_filesystem doc_root "Index.re.js" req);
         Dream.get "/style.css" (fun req ->
           Dream.from_filesystem doc_root "Index.re.css" req);
         Dream.get "/favicon.ico" (fun _ -> Dream.respond ~status:`No_Content "");
         Dream.get "/" (fun req ->
           let uuid = generate_uuid () in
           let* _ = Dream.sql req (Database.Todo.create_list uuid) in
           Dream.redirect req ("/" ^ uuid));
         Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
       ]
