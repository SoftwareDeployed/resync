open Lwt.Syntax
open Mutation_result

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
      (match thread_info with
       | Some _ ->
           let* () = Dream.sql request (Database.Chat.record_thread_view thread_id) in
           Lwt.return_some thread_id
       | None ->
           Lwt.return_some thread_id)

let stream_event_json ~event fields =
  Yojson.Basic.to_string
    (`Assoc
      [ ("type", `String "custom");
        ("payload", `Assoc (("event", `String event) :: fields)) ])

let broadcast_stream_event ~broadcast_fn ~thread_id ~event fields =
  let message = stream_event_json ~event fields in
  broadcast_fn thread_id (fun _ -> message)

let finalize_assistant_message ~request ~thread_id ~assistant_message_id ~full_response =
  if String.length full_response > 0 then
    Dream.sql request
      (Database.Chat.add_message
         assistant_message_id thread_id "assistant" full_response)
  else Lwt.return_unit

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
        let* () =
          broadcast_stream_event ~broadcast_fn ~thread_id ~event:"stream_error"
            [ ("thread_id", `String thread_id);
              ("message_id", `String assistant_message_id);
              ("error", `String (Printexc.to_string exn)) ]
        in
        Lwt.fail exn)
  in
  let body_stream = Cohttp_lwt.Body.to_stream resp_body in
  let ndjson_parser = NdjsonParser.make () in
  let all_tokens = ref [] in
  let* () =
    broadcast_stream_event ~broadcast_fn ~thread_id ~event:"stream_started"
      [ ("thread_id", `String thread_id);
        ("message_id", `String assistant_message_id) ]
  in
  let rec read_loop () =
    let* chunk = Lwt_stream.get body_stream in
    match chunk with
    | None ->
        let* () =
          broadcast_stream_event ~broadcast_fn ~thread_id ~event:"stream_complete"
            [ ("thread_id", `String thread_id);
              ("message_id", `String assistant_message_id) ]
        in
        let full_response = String.concat "" (List.rev !all_tokens) in
        finalize_assistant_message ~request ~thread_id ~assistant_message_id ~full_response
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
               | _ -> ())
               jsons;
             Lwt_list.iter_s
               (fun text ->
                 broadcast_stream_event ~broadcast_fn ~thread_id
                   ~event:"token_received"
                   [ ("thread_id", `String thread_id);
                     ("message_id", `String assistant_message_id);
                     ("token", `String text) ])
               (List.rev !tokens)
           with _ -> Lwt.return_unit)
        in
        read_loop ()
  in
  read_loop ()

let require_thread request thread_id f =
  let* thread = Dream.sql request (Database.Chat.get_thread thread_id) in
  match thread with
  | None -> Lwt.return (Ack (Error ("Thread not found: " ^ thread_id)))
  | Some _ -> f ()

let validate_mutation request action_json =
  Lwt.catch
    (fun () ->
      let kind =
        match Mutation_json.assoc "kind" action_json with
        | Some (`String k) -> k
        | _ -> ""
      in
      let action_opt =
        match kind with
        | "send_prompt" ->
            Some (LlmChatStore.SendPrompt { LlmChatStore.thread_id = ""; LlmChatStore.prompt = "" })
        | "delete_thread" -> Some (LlmChatStore.DeleteThread "")
        | "select_thread" -> Some (LlmChatStore.SelectThread "")
        | "create_new_thread" ->
            Some (LlmChatStore.CreateNewThread { LlmChatStore.id = ""; LlmChatStore.title = "" })
        | "set_input" -> Some (LlmChatStore.SetInput "")
        | "set_error" -> Some (LlmChatStore.SetError "")
        | _ -> None
      in
      match action_opt with
      | None -> Lwt.return (Ok ())
      | Some action ->
          let thread_id_opt =
            match Mutation_json.assoc "payload" action_json with
            | Some payload ->
                (match Mutation_json.required_string "thread_id" payload with
                 | Ok id -> Some id
                 | Error _ -> None)
            | None -> None
          in
          let* current_thread_id =
            match thread_id_opt with
            | Some thread_id ->
                let* thread = Dream.sql request (Database.Chat.get_thread thread_id) in
                Lwt.return (match thread with Some _ -> Some thread_id | None -> None)
            | None -> Lwt.return_none
          in
          let state : Model.t = {
            threads = [||];
            current_thread_id;
            messages = [||];
            input = "";
            updated_at = 0.0;
          } in
          (match StoreBuilder.GuardTree.resolve ~state ~action LlmChatStore.guardTree with
           | StoreRuntimeTypes.Allow -> Lwt.return (Ok ())
           | StoreRuntimeTypes.Deny reason -> Lwt.return (Error reason)))
    (function
      | Caqti_error.Exn error -> Lwt.return (Error (Caqti_error.show error))
      | exn -> Lwt.return (Error (Printexc.to_string exn)))

let handle_mutation broadcast_fn request ~db ~action_id ~mutation_name:_ action =
  let open Mutation_json in
  let kind =
    match assoc "kind" action with
    | Some (`String value) -> Ok value
    | _ -> Error "Missing action kind"
  in
  match kind with
  | Error error -> Lwt.return (Ack (Error error))
  | Ok "send_prompt" ->
      begin match assoc "payload" action with
      | None -> Lwt.return (Ack (Error "Missing send_prompt payload"))
      | Some payload ->
          begin match required_string "thread_id" payload with
          | Error error -> Lwt.return (Ack (Error error))
          | Ok thread_id ->
              begin match required_string "prompt" payload with
              | Error error -> Lwt.return (Ack (Error error))
              | Ok prompt ->
                  require_thread request thread_id (fun () ->
                    let message_id = UUID.make () in
                    let assistant_message_id = UUID.make () in
                    Lwt.catch
                      (fun () ->
                        let* () =
                          Database.Chat.add_message
                            message_id thread_id "user" prompt db
                        in
                        let () =
                          Lwt.async (fun () ->
                            Lwt.catch
                              (fun () ->
                                stream_ollama ~broadcast_fn ~request ~thread_id
                                  ~assistant_message_id ())
                              (fun exn ->
                                 broadcast_stream_event ~broadcast_fn ~thread_id
                                   ~event:"stream_error"
                                   [ ("thread_id", `String thread_id);
                                     ("message_id", `String assistant_message_id);
                                     ("error", `String (Printexc.to_string exn)) ]))
                        in
                        Lwt.return (Ack (Ok ())))
                      (function
                        | Caqti_error.Exn error ->
                            Lwt.return (Ack (Error (Caqti_error.show error)))
                        | exn ->
                            Lwt.return
                              (Ack (Error (Printexc.to_string exn)))))
              end
          end
      end
  | Ok "create_new_thread" ->
      begin match assoc "payload" action with
      | None -> Lwt.return (Ack (Error "Missing create_new_thread payload"))
      | Some payload ->
          begin match
            (required_string "id" payload, required_string "title" payload)
          with
          | (Ok thread_id, Ok title) ->
              let* result =
                Database.Chat.create_thread thread_id title db
              in
              (match result with
               | () ->
                   let* () =
                     Database.Chat.record_thread_view thread_id db
                   in
                   Lwt.return (Ack (Ok ()))
               | exception Caqti_error.Exn error ->
                   Lwt.return (Ack (Error (Caqti_error.show error)))
               | exception exn ->
                   Lwt.return (Ack (Error (Printexc.to_string exn))))
          | (Error error, _) | (_, Error error) ->
              Lwt.return (Ack (Error error))
          end
      end
  | Ok "set_error"
  | Ok "select_thread"
  | Ok "set_input" ->
      Lwt.return (Ack (Ok ()))
  | Ok "delete_thread" ->
      (match assoc "payload" action with
       | Some payload ->
           (match required_string "thread_id" payload with
            | Ok thread_id ->
                Lwt.catch
                  (fun () ->
                     let* () = Database.Chat.delete_thread thread_id db in
                     let delete_patch =
                       `Assoc [
                         ("type", `String "patch");
                         ("table", `String "threads");
                         ("action", `String "DELETE");
                         ("id", `String thread_id);
                       ] |> Yojson.Basic.to_string
                     in
                     let* () =
                       Lwt.catch
                         (fun () -> broadcast_fn thread_id (fun _ -> delete_patch))
                         (fun exn ->
                            Printf.eprintf "[delete_thread] broadcast failed: %s\n%!" (Printexc.to_string exn);
                            Lwt.return_unit)
                     in
                     Lwt.return (Ack (Ok ())))
                  (function
                   | Caqti_error.Exn error ->
                       Printf.eprintf "[delete_thread] DB error: %s\n%!" (Caqti_error.show error);
                       Lwt.return (Ack (Error (Caqti_error.show error)))
                   | exn ->
                       Printf.eprintf "[delete_thread] unexpected error: %s\n%!" (Printexc.to_string exn);
                       Lwt.return (Ack (Error (Printexc.to_string exn))))
            | Error error -> Lwt.return (Ack (Error error)))
       | None -> Lwt.return (Ack (Error "Missing delete_thread payload")))
  | Ok _ -> Lwt.return (Ack (Error "Unknown action kind"))

let () =
  let builder =
    Server_builder.make
      ~doc_root_var:"LLM_CHAT_DOC_ROOT"
      ~db_url_var:"DB_URL"
      ~default_interface:"127.0.0.1"
      ~default_port:8897
      ()
  in
  let doc_root = Server_builder.doc_root builder in
  let db_uri = Option.get (Server_builder.db_uri builder) in
  let adapter =
    Adapter.pack
      (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
      (Pgnotify_adapter.create ~db_uri ())
  in
  builder
  |> Server_builder.with_packed_adapter adapter
  |> Server_builder.with_middleware
    ~resolve_subscription
    ~load_snapshot:get_config_json
    ~handle_mutation
    ~validate_mutation
  |> Server_builder.with_routes [
    Dream.get "/static/**" (Dream.static doc_root);
    Dream.get "/app.js" (fun req -> Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req -> Dream.from_filesystem doc_root "Index.re.css" req);
    Dream.get "/favicon.ico" (fun _ -> Dream.respond ~status:`No_Content "");
    Dream.get "/" (fun req ->
      let* threads = Dream.sql req (Database.Chat.get_threads ()) in
      match Array.length threads > 0 with
      | true -> Dream.redirect req ("/" ^ threads.(0).id)
      | false ->
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
      let message_id = UUID.make () in
      let* _ = Dream.sql request (Database.Chat.add_message message_id thread_id role content) in
      Dream.respond (Yojson.Safe.to_string (`Assoc [("id", `String message_id); ("thread_id", `String thread_id)])));
    Dream.post "/api/test/delete-all-threads" (fun request ->
      let* _ = Dream.sql request (Database.Chat.delete_all_threads ()) in
      Dream.json "{\"status\":\"deleted_all\"}");
    Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
  ]
  |> Server_builder.run
