open Lwt.Syntax

let doc_root =
  match Sys.getenv_opt "ECOMMERCE_DOC_ROOT" with
  | Some doc_root -> doc_root
  | None -> failwith "ECOMMERCE_DOC_ROOT is required"

let db_uri =
  match Sys.getenv_opt "DB_URL" with
  | Some uri -> uri
  | None -> failwith "DB_URL is required"

let server_interface =
  match Sys.getenv_opt "SERVER_INTERFACE" with
  | Some interface -> interface
  | None -> "127.0.0.1"

(* Fetch config for a premise from database *)
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

let realtime_adapter =
  Adapter.pack
    (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
    (Pgnotify_adapter.create ~db_uri ())

let realtime_middleware =
  Middleware.create ~adapter:realtime_adapter ~resolve_subscription
    ~load_snapshot:get_config_json ()

let static_asset_path request =
  let path, _search = Dream.target(request) |> Dream.split_target in
  Filename.basename path

let () =
  (match Lwt_main.run (Adapter.start realtime_adapter) with
  | () -> ()
  | exception Failure msg ->
      Printf.eprintf "Failed to connect notification listener: %s\n" msg);
  Dream.run ~interface:server_interface ~port:8899 @@ Dream.logger
  @@ Dream.sql_pool ~size:50 db_uri
  @@ Dream.router
       [
           Middleware.route "_events" realtime_middleware;
           Dream.get "/static/**" (fun req ->
               Dream.from_filesystem doc_root (static_asset_path req) req);
           Dream.get "/app.js" (fun req ->
               Dream.from_filesystem doc_root "Index.re.js" req);
           Dream.get "/style.css" (fun req ->
               Dream.from_filesystem doc_root "Index.re.css" req);
         Dream.get "/" (UniversalRouterDream.handler ~app:EntryServer.app);
         Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
       ]
