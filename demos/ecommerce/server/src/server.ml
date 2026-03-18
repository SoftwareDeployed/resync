open Lwt.Syntax

let doc_root =
  Sys.getenv_opt "DOC_ROOT"
  |> Option.value ~default:"./_build/default/demos/ecommerce/ui/src/app/"

let db_uri =
  match (Sys.getenv_opt "DB_URL", Sys.getenv_opt "ALLOW_DEV_DB") with
  | Some(uri), _ -> uri
  | None, Some(value) -> (
      match String.lowercase_ascii value with
      | "1" | "true" | "yes" ->
          "postgres://executor:executor-password@127.0.0.1:5432/executor_db"
      | _ ->
          failwith
            "DB_URL is required unless ALLOW_DEV_DB is explicitly enabled")
  | None, None ->
      failwith
        "DB_URL is required unless ALLOW_DEV_DB is explicitly enabled"

(* let get_config premise_id = *)
(* let* premise = Database.Premise.get_premise premise_id in *)
(* let* inventory = Database.Inventory.get_list premise_id in *)
(* let response = Config.{ inventory; premise } in *)
(* Lwt.return response *)

let stream_react_app response_stream react_element =
  let* () = Dream.write response_stream "<!DOCTYPE html>" in
  let* stream, _abort = ReactDOM.renderToStream react_element in
  let* () =
    Lwt_stream.iter_s
      (fun chunk ->
        let* () = Dream.write response_stream chunk in
        Dream.flush response_stream)
      stream
  in
  Lwt.return ()

let handle_frontend request route_root premise =
  let premise_id = premise.PeriodList.Premise.id in
  let* inventory =
    if premise_id = "" then Lwt.return [||]
    else Dream.sql request (Database.Inventory.get_list premise_id)
  in
  let config : Config.t = { inventory; premise = Some premise } in
  let server_path = UniversalRouterDream.requestPath request in
  let server_search = UniversalRouterDream.requestSearch request in
  Dream.stream
    ~headers:[ ("Content-Type", "text/html") ]
    (fun response_stream ->
      let app_element =
        EntryServer.handler
          ~routeRoot:route_root
          ~serverPath:server_path
          ~serverSearch:server_search
          config
      in
      stream_react_app response_stream app_element)

let rec find_matching_frontend_route request candidate_roots =
  match candidate_roots with
  | [] -> Lwt.return_none
  | route_root :: remaining_roots -> (
      match
        UniversalRouterDream.matchRequest ~router:Routes.router ~routeRoot:route_root
          request
      with
      | None -> find_matching_frontend_route request remaining_roots
      | Some _ ->
          let* premise =
            Dream.sql request (Database.Premise.get_route_premise route_root)
          in
          (match premise with
          | Some premise -> Lwt.return_some (route_root, premise)
          | None -> find_matching_frontend_route request remaining_roots))

let frontend_handler request =
  let request_path = UniversalRouterDream.requestPath request in
  let candidate_roots = UniversalRouter.candidateRouteRoots(request_path) in
  let* matched_route = find_matching_frontend_route request candidate_roots in
  match matched_route with
  | Some (route_root, premise) -> handle_frontend request route_root premise
  | None -> Dream.empty `Not_Found

(*
  let read_whole_file file_path =
    Lwt_io.with_file file_path ~mode:Lwt_io.Input (fun channel ->
        Lwt_io.read channel)
  in
  let%lwt index_html = read_whole_file (doc_root ^ "/ui/index.html") in
  let initial_state = `Assoc [] in
  (* Build from actual state *)
  let html =
    render_app_html ~pathname:route_root ~html_placeholder:"<!--app-html-->"
      ~initial_state
  in
  Dream.respond ~headers:[ ("Content-Type", "text/html") ] html
*)
(*let handle_config premise_id =
 let%lwt config = get_config premise_id in
  let json = Config.{
    inventory = config.inventory;
    premise = config.premise;
  } |> Yojson.Safe.to_string in
  Dream.respond ~headers:["Content-Type", "application/json"] json
  *)

(* Fetch config for a premise from database *)
let get_config request premise_id =
  let* premise = Dream.sql request (Database.Premise.get_premise premise_id) in
  let* inventory = Dream.sql request (Database.Inventory.get_list premise_id) in
  let config : Config.t = { inventory; premise } in
  Lwt.return config

let get_config_json request premise_id =
  let* config = get_config request premise_id in
  Lwt.return (Store.serializeSnapshot config)

let resolve_subscription request selection =
  match RealtimeSubscription.decode_channel selection with
  | None -> Lwt.return_none
  | Some premise_id ->
      let* premise = Dream.sql request (Database.Premise.get_premise premise_id) in
      Lwt.return (Option.map (fun _ -> premise_id) premise)

let realtime_adapter =
  Adapter.pack
    (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
    (Pgnotify_adapter.create ~db_uri ())

let realtime_middleware =
  Middleware.create ~adapter:realtime_adapter
    ~resolve_subscription ~load_snapshot:get_config_json

let () =
  let () =
    match Lwt_main.run (Adapter.start realtime_adapter) with
    | () -> ()
    | exception Failure msg ->
        Printf.eprintf "Failed to connect notification listener: %s\n" msg
  in
  Dream.run ~port:8899 (*@@ Dream.livereload*)
  @@ Dream.logger
  @@ Dream.sql_pool ~size:50 db_uri
  (* @@ Dream.sql_sessions *)
  @@ Dream.router
       [
         Middleware.route "_events" realtime_middleware;
         Dream.get "/static/**" (Dream.static doc_root);
         Dream.get "/app.js" (fun req ->
             Dream.from_filesystem doc_root "Index.re.js" req);
         Dream.get "/style.css" (fun req ->
             Dream.from_filesystem doc_root "Index.re.css" req);
          Dream.get "/" frontend_handler;
          Dream.get "/**" frontend_handler;
         (*
  Dream.get "/config/:premise_id" (fun req ->
    let premise_id = Dream.param req "premise_id" in
    handle_config premise_id
  )
  *)
       ]
