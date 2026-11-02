let () =
  let doc_root =
    match Sys.getenv_opt "TODO_DOC_ROOT" with
    | Some doc_root -> doc_root
    | None -> failwith "TODO_DOC_ROOT is required"
  in
  Server_builder.make ~doc_root ~default_interface:"127.0.0.1" ~default_port:8080 ()
  |> Server_builder.with_routes (
       Server_builder.ui_routes ~doc_root
       @ [
           Dream.get "/" (UniversalRouterDream.handler ~app:EntryServer.app);
           Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
         ]
     )
  |> Server_builder.run
