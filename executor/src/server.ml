open Lwt.Syntax

let doc_root =
  Sys.getenv_opt "DOC_ROOT"
  |> Option.value ~default:"./_build/default/ui/src/app/"

let db_uri = Sys.getenv_opt "DB_URL"
|> Option.value ~default:"postgres://executor:executor-password@127.0.0.1:5432/executor_db"

(* let get_config premise_id = *)
  (* let* premise = Database.Premise.get_premise premise_id in *)  
  (* let* inventory = Database.Inventory.get_list premise_id in *)
  (* let response = Config.{ inventory; premise } in *)
  (* Lwt.return response *)

let stream_react_app response_stream react_element =
  let* stream, _abort = ReactDOM.renderToStream react_element in
  let* () =
    Lwt_stream.iter_s
      (fun chunk ->
        let* () = Dream.write response_stream chunk in
        Dream.flush response_stream)
      stream
  in
  Lwt.return ()

let handle_frontend request route_root =
  let* premise = Dream.sql request (Database.Premise.get_route_premise route_root) in
  let premise_id = 
    match premise with
    | None -> ""
    | Some(p) -> p.id
  in
  let* inventory = 
    if premise_id = "" then
      Lwt.return [||]
    else
      Dream.sql request (Database.Inventory.get_list premise_id)
  in
  let config: Config.t = {
    inventory = inventory;
    premise = premise
  } in 
  Dream.stream
    ~headers:[ ("Content-Type", "text/html") ]
    (fun response_stream ->
      let app_element = EntryServer.handler config in
      stream_react_app response_stream app_element)

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
  let config: Config.t = {
    inventory = inventory;
    premise = premise
  } in
  Lwt.return config

(* WebSocket handler for real-time updates *)
let websocket_handler request websocket =
  let rec loop () =
    match%lwt Dream.receive websocket with
    | None -> Lwt.return ()
    | Some msg ->
        let* () = Database.Bus.handle_message request get_config websocket msg in
        loop ()
  in
  loop ()

let () =
  (* Initialize the dedicated PostgreSQL notification connection *)
  let () = 
    match Lwt_main.run (Database.PgNotifier.connect db_uri) with
    | Ok () -> 
        Database.PgNotifier.start ()
    | Error (`Msg msg) -> 
        Printf.eprintf "Failed to connect notification listener: %s\n" msg
  in
  Dream.run ~port:8899 @@ Dream.livereload @@ Dream.logger
  @@ Dream.sql_pool ~size:50 db_uri
  (* @@ Dream.sql_sessions *)
  @@ Dream.router
       [
         Dream.get "_events" (fun req -> Dream.websocket (fun ws -> websocket_handler req ws));
         Dream.get "/static/**" (Dream.static doc_root);
         Dream.get "/app.js" (fun req ->
             Dream.from_filesystem doc_root "Index.re.js" req);
         Dream.get "/style.css" (fun req ->
             Dream.from_filesystem doc_root "Index.re.css" req);
         Dream.get "/" (fun req ->
             let route_root = "/" in
             handle_frontend req route_root);
         (*
  Dream.get "/config/:premise_id" (fun req ->
    let premise_id = Dream.param req "premise_id" in
    handle_config premise_id
  )
  *)
       ]
