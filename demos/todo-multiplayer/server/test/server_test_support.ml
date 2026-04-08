open Lwt.Syntax

open Caqti_request.Infix

let create_list_query =
  (Caqti_type.string ->. Caqti_type.unit)
    RealtimeSchema.Mutations.CreateList.sql

let create_list uuid (module Db : Caqti_lwt.CONNECTION) =
  let* result = Db.exec create_list_query uuid in
  Caqti_lwt.or_fail result

let generate_uuid () =
  Random.self_init ();
  let random_hex len =
    let buf = Buffer.create len in
    for _ = 1 to len do
      let n = Random.int 16 in
      let chars = "0123456789abcdef" in
      Buffer.add_char buf chars.[n]
    done;
    Buffer.contents buf
  in
  Printf.sprintf "%s-%s-%s-%s-%s" (random_hex 8) (random_hex 4) (random_hex 4)
    (random_hex 4) (random_hex 12)

let handler ~doc_root ~db_uri =
  Dream.logger @@ Dream.sql_pool ~size:10 db_uri
  @@ Dream.router
       [ Dream.get "/static/**" (Dream.static doc_root);
         Dream.get "/app.js" (fun req -> Dream.from_filesystem doc_root "Index.re.js" req);
         Dream.get "/style.css" (fun req -> Dream.from_filesystem doc_root "Index.re.css" req);
         Dream.get "/favicon.ico" (fun _ -> Dream.respond ~status:`No_Content "");
         Dream.get "/" (fun req ->
             let uuid = generate_uuid () in
             let* _ = Dream.sql req (create_list uuid) in
             Dream.redirect req ("/" ^ uuid));
         Dream.get "/**" (fun _ -> Dream.respond ~status:`Not_Found "") ]
