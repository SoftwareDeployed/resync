open Lwt.Syntax

let doc_root =
  match Sys.getenv_opt "LLM_CHAT_DOC_ROOT" with
  | Some doc_root -> doc_root
  | None -> failwith "LLM_CHAT_DOC_ROOT is required"

let db_uri =
  match Sys.getenv_opt "DB_URL" with
  | Some uri -> uri
  | None -> failwith "DB_URL is required"

let server_interface =
  match Sys.getenv_opt "SERVER_INTERFACE" with
  | Some interface -> interface
  | None -> "127.0.0.1"

let server_port =
  match Sys.getenv_opt "SERVER_PORT" with
  | Some port -> int_of_string port
  | None -> 8897

let get_config request thread_id =
  let* thread_info = Dream.sql request (Database.Chat.get_thread thread_id) in
  let* messages = Dream.sql request (Database.Chat.get_messages thread_id) in
  let* threads = Dream.sql request (Database.Chat.get_threads ()) in
  let state : Model.t = {
    threads;
    current_thread_id = Some thread_id;
    messages;
    input = "";
    updated_at =
      (match thread_info with
       | Some thread -> thread.updated_at
       | None -> 0.0);
  } in
  Lwt.return state

let get_config_json request thread_id =
  let* config = get_config request thread_id in
  Lwt.return (LlmChatStore.serializeSnapshot config)

let resolve_subscription request selection =
  match RealtimeSubscription.decode_channel selection with
  | None -> Lwt.return_none
  | Some thread_id ->
      let* thread_info = Dream.sql request (Database.Chat.get_thread thread_id) in
      let* () = Dream.sql request (Database.Chat.record_thread_view thread_id) in
      Lwt.return (Option.map (fun _ -> thread_id) thread_info)

let realtime_adapter =
  Adapter.pack
    (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
    (Pgnotify_adapter.create ~db_uri ())

let assoc key = function
  | `Assoc fields -> List.assoc_opt key fields
  | _ -> None

let required_string key json =
  match assoc key json with
  | Some (`String value) -> Ok value
  | _ -> Error ("Missing string field: " ^ key)

let stream_ollama ~broadcast_fn ~request ~thread_id ~assistant_message_id () =
  let* messages = Dream.sql request (Database.Chat.get_messages thread_id) in
  let messages_json =
    Array.to_list messages
    |> List.map (fun (msg: Model.Message.t) ->
        `Assoc [("role", `String msg.role); ("content", `String msg.content)])
  in
  let ollama_body = Yojson.Safe.to_string (`Assoc [
    ("model", `String "llama3.1");
    ("messages", `List messages_json);
    ("stream", `Bool true)
  ]) in
  let* (_resp, resp_body) =
    Lwt.catch
      (fun () ->
        Cohttp_lwt_unix.Client.post
          ~body:(Cohttp_lwt.Body.of_string ollama_body)
          ~headers:(Cohttp.Header.of_list [("Content-Type", "application/json")])
          (Uri.of_string "http://localhost:11434/api/chat"))
      (fun exn ->
        let error_msg = Yojson.Basic.to_string (`Assoc [
          ("type", `String "custom");
          ("payload", `Assoc [
            ("event", `String "stream_error");
            ("thread_id", `String thread_id);
            ("message_id", `String assistant_message_id);
            ("error", `String (Printexc.to_string exn))
          ])
        ]) in
        let* () = broadcast_fn thread_id (fun _ -> error_msg) in
        Lwt.fail exn)
  in
  let body_stream = Cohttp_lwt.Body.to_stream resp_body in
  let ndjson_parser = NdjsonParser.make () in
  let all_tokens = ref [] in
  let start_msg = Yojson.Basic.to_string (`Assoc [
    ("type", `String "custom");
    ("payload", `Assoc [
      ("event", `String "stream_started");
      ("thread_id", `String thread_id);
      ("message_id", `String assistant_message_id)
    ])
  ]) in
  let* () = broadcast_fn thread_id (fun _ -> start_msg) in
  let rec read_loop () =
    let* chunk = Lwt_stream.get body_stream in
    match chunk with
    | None ->
      let done_msg = Yojson.Basic.to_string (`Assoc [
        ("type", `String "custom");
        ("payload", `Assoc [
          ("event", `String "stream_complete");
          ("thread_id", `String thread_id);
          ("message_id", `String assistant_message_id)
        ])
      ]) in
      let* () = broadcast_fn thread_id (fun _ -> done_msg) in
      let full_response = String.concat "" (List.rev !all_tokens) in
      if String.length full_response > 0 then
        let action_id = UUID.make () in
        Dream.sql request
          (Database.Chat.add_message (action_id, assistant_message_id, thread_id, "assistant", full_response))
      else Lwt.return_unit
    | Some chunk ->
      let* () =
        (try
          let jsons = NdjsonParser.feed ndjson_parser chunk in
          let tokens = ref [] in
          Array.iter (fun json ->
            match json with
            | `Assoc fields ->
                (match List.assoc_opt "done" fields with
                 | Some (`Bool true) -> ()
                 | _ ->
                   match List.assoc_opt "message" fields with
                   | Some (`Assoc msg_fields) ->
                       (match List.assoc_opt "content" msg_fields with
                        | Some (`String text) when String.length text > 0 ->
                            tokens := text :: !tokens;
                            all_tokens := text :: !all_tokens
                        | _ -> ())
                   | _ -> ())
            | _ -> ()
          ) jsons;
          Lwt_list.iter_s (fun text ->
            let token_msg = Yojson.Basic.to_string (`Assoc [
              ("type", `String "custom");
              ("payload", `Assoc [
                ("event", `String "token_received");
                ("thread_id", `String thread_id);
                ("message_id", `String assistant_message_id);
                ("token", `String text)
              ])
            ]) in
            broadcast_fn thread_id (fun _ -> token_msg)
          ) (List.rev !tokens)
        with _ -> Lwt.return_unit)
      in
      read_loop ()
  in
  read_loop ()

let handle_mutation broadcast_fn request ~action_id action =
  let kind =
    match assoc "kind" action with
    | Some (`String value) -> Ok value
    | _ -> Error "Missing action kind"
  in
  match kind with
  | Error error -> Lwt.return (Middleware.Ack (Error error))
  | Ok "send_prompt" ->
      (match assoc "payload" action with
       | Some payload ->
           (match (required_string "thread_id" payload, required_string "prompt" payload) with
             | Ok thread_id, Ok prompt ->
                 let message_id = UUID.make () in
                 let assistant_message_id = UUID.make () in
                 let* result =
                   Dream.sql request
                     (Database.Chat.add_message (action_id, message_id, thread_id, "user", prompt))
                 in
                 (match result with
                  | () ->
                      Lwt.async (fun () ->
                        Lwt.catch
                          (fun () -> stream_ollama ~broadcast_fn ~request ~thread_id ~assistant_message_id ())
                          (fun exn ->
                            let error_msg = Yojson.Basic.to_string (`Assoc [
                              ("type", `String "custom");
                              ("payload", `Assoc [
                                ("event", `String "stream_error");
                                ("thread_id", `String thread_id);
                                ("message_id", `String assistant_message_id);
                                ("error", `String (Printexc.to_string exn))
                              ])
                            ]) in
                            broadcast_fn thread_id (fun _ -> error_msg)));
                      Lwt.return (Middleware.Ack (Ok ()))
                  | exception Caqti_error.Exn error ->
                      Lwt.return (Middleware.Ack (Error (Caqti_error.show error)))
                  | exception exn ->
                      Lwt.return (Middleware.Ack (Error (Printexc.to_string exn))))
              | Error error, _ | _, Error error ->
                  Lwt.return (Middleware.Ack (Error error)))
         | None -> Lwt.return (Middleware.Ack (Error "Missing send_prompt payload")))
  | Ok "set_error"
  | Ok "select_thread"
  | Ok "set_input" ->
      Lwt.return (Middleware.Ack (Ok ()))
  | Ok "delete_thread" ->
      (match assoc "payload" action with
       | Some payload ->
           (match required_string "thread_id" payload with
            | Ok thread_id ->
                let* result =
                  Dream.sql request (Database.Chat.delete_thread thread_id)
                in
                (match result with
                 | () -> Lwt.return (Middleware.Ack (Ok ()))
                 | exception Caqti_error.Exn error ->
                     Lwt.return (Middleware.Ack (Error (Caqti_error.show error)))
                 | exception exn ->
                     Lwt.return (Middleware.Ack (Error (Printexc.to_string exn))))
            | Error error -> Lwt.return (Middleware.Ack (Error error)))
       | None -> Lwt.return (Middleware.Ack (Error "Missing delete_thread payload")))
  | Ok _ -> Lwt.return (Middleware.Ack (Error "Unknown action kind"))

let realtime_middleware =
  Middleware.create ~adapter:realtime_adapter ~resolve_subscription
    ~load_snapshot:get_config_json ~handle_mutation ()

let () =
  (match Lwt_main.run (Adapter.start realtime_adapter) with
   | () -> ()
   | exception Failure msg ->
       Printf.eprintf "Failed to connect notification listener: %s\n" msg);
  Dream.run ~interface:server_interface ~port:server_port @@ Dream.logger
  @@ Dream.sql_pool ~size:10 db_uri
  @@ Dream.router
       [ Middleware.route "_events" realtime_middleware;
         Dream.get "/static/**" (Dream.static doc_root);
         Dream.get "/app.js" (fun req -> Dream.from_filesystem doc_root "Index.re.js" req);
         Dream.get "/style.css" (fun req -> Dream.from_filesystem doc_root "Index.re.css" req);
          Dream.get "/favicon.ico" (fun _ -> Dream.respond ~status:`No_Content "");
          Dream.get "/" (fun req ->
            let uuid = UUID.make () in
            let* _ = Dream.sql req (Database.Chat.create_thread uuid "New Chat") in
            Dream.redirect req ("/" ^ uuid));
          Dream.post "/api/test/delete-thread/:thread_id" (fun request ->
            let thread_id = Dream.param request "thread_id" in
            let* _ = Dream.sql request (Database.Chat.delete_thread thread_id) in
            Dream.json "{\"status\":\"deleted\"}");
          Dream.post "/api/test/create-thread" (fun request ->
            let* body = Dream.body request in
            let json = Yojson.Safe.from_string body in
            let title = try Yojson.Safe.Util.(to_string (member "title" json)) with _ -> "Test Thread" in
            let uuid = UUID.make () in
            let* _ = Dream.sql request (Database.Chat.create_thread uuid title) in
            Dream.respond (Yojson.Safe.to_string (`Assoc [("id", `String uuid); ("title", `String title)])));
          Dream.post "/api/test/add-message" (fun request ->
            let* body = Dream.body request in
            let json = Yojson.Safe.from_string body in
            let thread_id = try Yojson.Safe.Util.(to_string (member "thread_id" json)) with _ -> "" in
            let role = try Yojson.Safe.Util.(to_string (member "role" json)) with _ -> "assistant" in
            let content = try Yojson.Safe.Util.(to_string (member "content" json)) with _ -> "test message" in
            let action_id = UUID.make () in
            let message_id = UUID.make () in
            let* _ = Dream.sql request (Database.Chat.add_message (action_id, message_id, thread_id, role, content)) in
            Dream.respond (Yojson.Safe.to_string (`Assoc [("id", `String message_id); ("thread_id", `String thread_id)])));
          Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app) ]
