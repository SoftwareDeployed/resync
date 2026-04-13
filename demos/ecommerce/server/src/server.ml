open Lwt.Syntax

let get_config request premise_id =
  let* premise = Dream.sql request (Database.Premise.get_premise premise_id) in
  let* inventory = Dream.sql request (Database.Inventory.get_list premise_id) in
  let config : Model.t = {inventory; premise} in
  Lwt.return config

let get_config_json request premise_id =
  let* config = get_config request premise_id in
  Lwt.return (Store.serializeSnapshot config)

let resolve_subscription request selection =
  match RealtimeSubscription.decode_channel selection with
  | None -> Lwt.return_none
  | Some premise_id ->
      let* premise =
        Dream.sql request (Database.Premise.get_premise premise_id)
      in
      Lwt.return (Option.map (fun _ -> premise_id) premise)

let static_asset_path request =
  let path, _search = Dream.target(request) |> Dream.split_target in
  Filename.basename path

let () =
  let builder =
    Server_builder.make
      ~doc_root_var:"ECOMMERCE_DOC_ROOT"
      ~db_url_var:"DB_URL"
      ~default_interface:"127.0.0.1"
      ~default_port:8899
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
  |> Server_builder.with_routes [
    Dream.get "/static/**" (fun req ->
      Dream.from_filesystem doc_root (static_asset_path req) req);
    Dream.get "/app.js" (fun req ->
      Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req ->
      Dream.from_filesystem doc_root "Index.re.css" req);
    Dream.get "/api/test/inventory/:id/quantity/:quantity" (fun req ->
      let item_id = Dream.param req "id" in
      let quantity = Dream.param req "quantity" |> int_of_string in
      let* () = Dream.sql req (Database.Inventory.update_quantity item_id quantity) in
      Dream.respond ~status:`OK "updated");
    Dream.get "/" (UniversalRouterDream.handler ~app:EntryServer.app);
    Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
  ]
  |> Server_builder.run
