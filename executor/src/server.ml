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
(*
            premise.id,
            premise.name,
            premise.description,
            EXTRACT(EPOCH FROM premise.updated_at) AS updated_at
            *)
  let* premise = Dream.sql request (Database.Premise.get_route_premise route_root) in
  let (premise_id, name, description, updated_at) = Option.value premise ~default:("", "", "", 0.0) in
  let config: Config.t = {
    inventory = [||];
    premise = Some({
      id = premise_id;
      name = name;
      description = description;
      updated_at = updated_at
    })
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

(*
let websocket_handler req ws =
  let open Dream_websocket in
  let state = ref (Some ws) in
  let send msg = match !state with
    | Some ws -> send ws msg
    | None -> ()
  in
  let receive msg = match msg with
    | "ping" -> send "pong"
    | msg when String.starts_with ~prefix:"select " msg ->
      let premise_id = String.sub 7 (String.length msg - 7) |> String.trim in
      Database.Bus.with_listener premise_id
        ~on_message:(fun _message ->
          let* config = get_config premise_id in
          let json = Yojson.Safe.to_string config in
          send json
        );
      ()
    | _ -> ()
  in
  let finally () = state := None in
  Dream_websocket.websocket ~finally receive
*)

let () =
  Dream.run ~port:8899 @@ Dream.livereload @@ Dream.logger
  @@ Dream.sql_pool ~size:50 db_uri
  (* @@ Dream.sql_sessions *)
  @@ Dream.router
       [
         (*
  |> Dream.get "/ws" websocket_handler  
(fun _ ->
        Dream.websocket (fun websocket ->
          match%lwt Dream.receive websocket with
          | Some "Hello?" ->
            Dream.send websocket "Good-bye!"
          | _ ->
            Dream.close_websocket websocket));  |> Dream.get "/" handle_frontend
          *)
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
