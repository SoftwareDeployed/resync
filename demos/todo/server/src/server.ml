let () =
  let doc_root =
    match Sys.getenv_opt "TODO_DOC_ROOT" with
    | Some doc_root -> doc_root
    | None -> failwith "TODO_DOC_ROOT is required"
  in
  Server_builder.make ~doc_root ~default_interface:"127.0.0.1" ~default_port:8080 ()
  |> Server_builder.with_routes [
    Dream.get "/app.js" (fun req ->
      Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req ->
      Dream.from_filesystem doc_root "Index.re.css" req);
    Dream.get "/" (UniversalRouterDream.handler ~app:EntryServer.app);
    Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
  ]
  |> Server_builder.run
