open Lwt.Syntax

let doc_root =
  match Sys.getenv_opt "TODO_DOC_ROOT" with
  | Some doc_root -> doc_root
  | None -> failwith "TODO_DOC_ROOT is required"

let server_interface =
  match Sys.getenv_opt "SERVER_INTERFACE" with
  | Some interface -> interface
  | None -> "127.0.0.1"

let () =
  Dream.run ~interface:server_interface ~port:8080 @@ Dream.logger
  @@ Dream.router
       [
         Dream.get "/app.js" (fun req ->
             Dream.from_filesystem doc_root "Index.re.js" req);
          Dream.get "/style.css" (fun req ->
              Dream.from_filesystem doc_root "Index.re.css" req);
         Dream.get "/" (UniversalRouterDream.handler ~app:EntryServer.app);
         Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
       ]
