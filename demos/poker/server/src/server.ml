open Lwt.Syntax

let configured_model () =
  match Sys.getenv_opt "JEV_MODEL" with
  | Some model when String.trim model <> "" -> String.trim model
  | _ -> "jev-latest"

let configured_key () =
  match Sys.getenv_opt "TYPESAFE_API_KEY" with
  | Some key when String.trim key <> "" -> Some key
  | _ -> None

let json_error status message =
  Dream.json ~status (Yojson.Safe.to_string (`Assoc [ ("error", `String message) ]))

let status_handler _request =
  Dream.json
    (Yojson.Safe.to_string
       (`Assoc
         [ ("available", `Bool (configured_key () <> None));
           ("model", `String (configured_model ())) ]))

let upstream_decision ~key body =
  let operation =
    Lwt.catch
      (fun () ->
         let* response, response_body =
           Cohttp_lwt_unix.Client.post
             ~body:(Cohttp_lwt.Body.of_string body)
             ~headers:
               (Cohttp.Header.of_list
                  [ ("Content-Type", "application/json");
                    ("Accept", "application/json");
                    ("Authorization", "Bearer " ^ key) ])
             (Uri.of_string "https://api.typesafe.ai/v1/systemone")
         in
         let status = Cohttp.Response.status response |> Cohttp.Code.code_of_status in
         let* response_body = Cohttp_lwt.Body.to_string response_body in
         if status < 200 || status >= 300 then Lwt.return (Error `Upstream)
         else Lwt.return (Ok response_body))
      (fun _exn -> Lwt.return (Error `Upstream))
  in
  let timeout =
    let* () = Lwt_unix.sleep 25.0 in
    Lwt.return (Error `Timeout)
  in
  Lwt.pick [ operation; timeout ]

let decision_handler request =
  match configured_key () with
  | None -> json_error `Service_Unavailable "Jev is not configured; use local opponents."
  | Some key ->
      let* body = Dream.body request in
      (match JevDecision.parse_request body with
       | Error message -> json_error `Bad_Request message
       | Ok parsed ->
           let model = configured_model () in
           let upstream_body = JevDecision.upstream_body ~model parsed in
           let* response = upstream_decision ~key upstream_body in
           (match response with
            | Error `Timeout ->
                json_error `Gateway_Timeout "Jev did not respond; use local opponents."
            | Error `Upstream ->
                json_error `Bad_Gateway "Jev is unavailable; use local opponents."
            | Ok response_body ->
                (match JevDecision.parse_response ~request:parsed response_body with
                 | Error message -> json_error `Bad_Gateway message
                 | Ok decision ->
                     Dream.json (Yojson.Safe.to_string (JevDecision.decision_to_json decision)))))

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
       [Dream.get "/static/sounds/**" (Dream.static "demos/poker/ui/src/static/sounds")]
       @ Server_builder.ui_routes ~doc_root
       @ [
           Dream.get "/api/poker/jev/status" status_handler;
           Dream.post "/api/poker/jev/decision" decision_handler;
           Dream.get "/" (ssr);
           Dream.get "/**" (ssr);
         ]
     )
  |> Server_builder.run
