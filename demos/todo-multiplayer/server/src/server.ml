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

let handle_mutation request name payload =
  let json = Yojson.Basic.from_string payload in
  let get_field f = function
    | `Assoc fields ->
        (try List.assoc f fields with Not_found -> `Null)
    | _ -> `Null
  in
  match name with
  | "add_todo" ->
      let id = match get_field "id" json with `String s -> s | _ -> "" in
      let list_id = match get_field "list_id" json with `String s -> s | _ -> "" in
      let text = match get_field "text" json with `String s -> s | _ -> "" in
      let* () = Dream.sql request (Database.Todo.add_todo (id, list_id, text)) in
      Lwt.return ()
  | "toggle_todo" ->
      let id = match get_field "id" json with `String s -> s | _ -> "" in
      let* () = Dream.sql request (Database.Todo.toggle_todo id) in
      Lwt.return ()
  | "remove_todo" ->
      let id = match get_field "id" json with `String s -> s | _ -> "" in
      let* () = Dream.sql request (Database.Todo.remove_todo id) in
      Lwt.return ()
  | _ -> Lwt.return ()

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
  Dream.run ~interface:server_interface ~port:8898 @@ Dream.logger
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
