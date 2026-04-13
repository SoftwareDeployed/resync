open Lwt.Syntax
open Mutation_result

let get_config request list_id =
  let* list_info =
    Dream.sql request (fun db ->
      RealtimeSchema.Queries.GetListInfo.find_opt
        db
        RealtimeSchema.Queries.GetListInfo.caqti_type
        list_id)
  in
  let* todos =
    Dream.sql request (fun db ->
      RealtimeSchema.Queries.GetList.collect
        db
        RealtimeSchema.Queries.GetList.caqti_type
        list_id)
  in
  let todos_arr =
    Array.map
      (fun (row : RealtimeSchema.Queries.GetList.row) ->
         ({ Model.Todo.id = row.id; list_id = row.list_id; text = row.text; completed = row.completed }
           : Model.Todo.t))
      (Array.of_list todos)
  in
  let list =
    Option.map
      (fun (row : RealtimeSchema.Queries.GetListInfo.row) ->
         ({ Model.TodoList.id = row.id; name = row.name; updated_at = row.updated_at } : Model.TodoList.t))
      list_info
  in
  let config : Model.t = { todos = todos_arr; list } in
  Lwt.return config

let get_config_json request list_id =
  let* config = get_config request list_id in
  Lwt.return (TodoStore.serializeSnapshot config)

let resolve_subscription request selection =
  match RealtimeSubscription.decode_channel selection with
  | None -> Lwt.return_none
  | Some list_id ->
      let* list_info =
        Dream.sql request (fun db ->
          RealtimeSchema.Queries.GetListInfo.find_opt
            db
            RealtimeSchema.Queries.GetListInfo.caqti_type
            list_id)
      in
      Lwt.return (Option.map (fun _ -> list_id) list_info)

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

let fail_query_like_mutation () =
  let query =
    Caqti_request.Infix.(
      (Caqti_type.unit ->. Caqti_type.unit)
        ({sql|
          DO $$
          BEGIN
            PERFORM 1 / 0;
          END
          $$;
        |sql}))
  in
  fun (module Db : Caqti_lwt.CONNECTION) ->
  let* result = Db.exec query () in
  Caqti_lwt.or_fail result

let handle_mutation _broadcast_fn _request ~db ~action_id ~mutation_name:_ action =
  let open Mutation_json in
  let kind =
    match assoc "kind" action with
    | Some (`String value) -> Ok value
    | _ -> Error "Missing action kind"
  in
  match kind with
  | Error error -> Lwt.return (Ack (Error error))
  | Ok "fail_server_mutation" ->
      let* result =
        Mutation_json.mutation_result ~action_id
          (fail_query_like_mutation () db)
      in
      Mutation_json.finish_mutation_result ~action_id result
  | Ok "fail_client_mutation" ->
      Mutation_json.finish_mutation_result ~action_id
        (Error (Mutation_json.Client_error "Mutation failed from OCaml"))
  | Ok _ -> Lwt.return (Ack (Error "Unknown action kind"))

let () =
  Random.self_init ();
  let builder =
    Server_builder.make
      ~doc_root_var:"TODO_MP_DOC_ROOT"
      ~db_url_var:"DB_URL"
      ~default_interface:"127.0.0.1"
      ~default_port:8898
      ()
  in
  let doc_root = Server_builder.doc_root builder in
  let db_uri = Option.get (Server_builder.db_uri builder) in
  let adapter =
    Adapter.pack
      (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
      (Pgnotify_adapter.create ~db_uri ())
  in
  builder
  |> Server_builder.with_packed_adapter adapter
  |> Server_builder.with_middleware
    ~resolve_subscription
    ~load_snapshot:get_config_json
    ~dispatch_mutation:RealtimeSchema.dispatch_mutation
    ~handle_mutation
  |> Server_builder.with_routes [
    Dream.get "/static/**" (Dream.static doc_root);
    Dream.get "/app.js" (fun req -> Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req -> Dream.from_filesystem doc_root "Index.re.css" req);
    Dream.get "/favicon.ico" (fun _ -> Dream.respond ~status:`No_Content "");
    Dream.get "/" (fun req ->
      let uuid = generate_uuid () in
      let* _ = Dream.sql req (fun db ->
        RealtimeSchema.Mutations.CreateList.exec db uuid) in
      Dream.redirect req ("/" ^ uuid));
    Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
  ]
  |> Server_builder.run
