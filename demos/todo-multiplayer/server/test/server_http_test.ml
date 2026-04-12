let db_uri () =
  match Sys.getenv_opt "DB_URL" with
  | Some value -> value
  | None -> failwith "DB_URL is required for todo-multiplayer server tests"

let doc_root () =
  match Sys.getenv_opt "TODO_MP_DOC_ROOT" with
  | Some value -> value
  | None -> failwith "TODO_MP_DOC_ROOT is required for todo-multiplayer server tests"

let make_handler () =
  Server_test_support.handler ~doc_root:(doc_root ()) ~db_uri:(db_uri ())

let run_request target =
  Dream.test (make_handler ()) (Dream.request ~target "")

let suite =
  ( "server_http",
    [
      Alcotest.test_case "GET / redirects to a created list" `Quick (fun () ->
          let response = run_request "/" in
          match Dream.header response "Location" with
          | Some location when Dream.status response = `See_Other && String.length location > 1 ->
              ()
          | Some location -> Alcotest.fail ("Unexpected redirect target: " ^ location)
          | None -> Alcotest.fail "Expected redirect location");
      Alcotest.test_case "GET invalid uuid route returns not found" `Quick (fun () ->
          let response = run_request "/not-a-uuid" in
          if Dream.status response = `Not_Found then ()
          else Alcotest.fail "Expected 404 for invalid uuid route");
      Alcotest.test_case "GET /favicon.ico returns no content" `Quick (fun () ->
          let response = run_request "/favicon.ico" in
          if Dream.status response = `No_Content then ()
          else Alcotest.fail "Expected 204 for favicon route");
    ] )
