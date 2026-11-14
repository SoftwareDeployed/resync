open Lwt.Syntax

let ssr request =
  let* response = UniversalRouterDream.handler ~app:EntryServer.app request in
  Dream.set_header response "Content-Type" "text/html; charset=utf-8";
  Lwt.return response

let () =
  let doc_root =
    match Sys.getenv_opt "POKER_DOC_ROOT" with
    | Some doc_root -> doc_root
    | None -> failwith "POKER_DOC_ROOT is required"
  in
  Server_builder.make
    ~doc_root
    ~port_var:"POKER_SERVER_PORT"
    ~default_interface:"127.0.0.1"
    ~default_port:8090
    ()
  |> Server_builder.with_routes (
       Server_builder.ui_routes ~doc_root
       @ [
           Dream.get "/" (ssr);
           Dream.get "/**" (ssr);
         ]
     )
  |> Server_builder.run
