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

let init () =
  Test_framework.describe "todo multiplayer http routes" (fun () ->
      Test_framework.test "GET / redirects to a created list" (fun () ->
          let response = run_request "/" in
          match Dream.header response "Location" with
          | Some location when Dream.status response = `See_Other && String.length location > 1 ->
              Test_framework.pass ()
          | Some location -> Test_framework.fail ("Unexpected redirect target: " ^ location)
          | None -> Test_framework.fail "Expected redirect location"
      );
      Test_framework.test "GET invalid uuid route returns not found" (fun () ->
          let response = run_request "/not-a-uuid" in
          if Dream.status response = `Not_Found then Test_framework.pass ()
          else Test_framework.fail "Expected 404 for invalid uuid route"
      );
      Test_framework.test "GET /favicon.ico returns no content" (fun () ->
          let response = run_request "/favicon.ico" in
          if Dream.status response = `No_Content then Test_framework.pass ()
          else Test_framework.fail "Expected 204 for favicon route"
      ))
