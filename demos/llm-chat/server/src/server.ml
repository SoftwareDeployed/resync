open Lwt.Syntax
open Mutation_result

let touch_thread_updated_at =
  let query =
    Caqti_request.Infix.(
      Caqti_type.string ->. Caqti_type.unit
    ) "UPDATE threads SET updated_at = NOW() WHERE id = $1::uuid"
  in
  fun (module Db : Caqti_lwt.CONNECTION) thread_id ->
    let* result = Db.exec query thread_id in
    Caqti_lwt.or_fail result

let collect_thread_ids =
  let query =
    Caqti_request.Infix.(
      Caqti_type.unit ->* Caqti_type.string
    ) "SELECT id::text FROM threads"
  in
  fun (module Db : Caqti_lwt.CONNECTION) ->
    let* result = Db.collect_list query () in
    Caqti_lwt.or_fail result

let collect_active_thread_view_ids =
  let query =
    Caqti_request.Infix.(
      Caqti_type.unit ->* Caqti_type.string
    ) "SELECT DISTINCT thread_id::text FROM active_thread_views"
  in
  fun (module Db : Caqti_lwt.CONNECTION) ->
    let* result = Db.collect_list query () in
    Caqti_lwt.or_fail result

let notify_query =
  Caqti_request.Infix.(
    Caqti_type.(t2 string string) ->. Caqti_type.unit
  ) "SELECT pg_notify($1, $2)"

let notify_patch (module Db : Caqti_lwt.CONNECTION) ~channel ~payload =
  let* result = Db.exec notify_query (channel, payload) in
  Caqti_lwt.or_fail result

let thread_delete_patch thread_id =
  Yojson.Safe.to_string
    (`Assoc
      [ ("type", `String "patch");
        ("table", `String "threads");
        ("id", `String thread_id);
        ("action", `String "DELETE") ])

let notify_deleted_threads db ~channels ~thread_ids =
  let channels = List.sort_uniq String.compare channels in
  Lwt_list.iter_s
    (fun channel ->
       Lwt_list.iter_s
         (fun thread_id ->
            notify_patch db ~channel ~payload:(thread_delete_patch thread_id))
         thread_ids)
    channels

let get_config request thread_id =
  let* thread_info_row =
    Dream.sql request (fun db ->
      RealtimeSchema.Queries.GetThread.find_opt
        db
        RealtimeSchema.Queries.GetThread.caqti_type
        thread_id)
  in
  let thread_info =
    Option.map
      (fun (row : RealtimeSchema.Queries.GetThread.row) ->
         ({ Model.Thread.id = row.id; title = row.title; updated_at = row.updated_at } : Model.Thread.t))
      thread_info_row
  in
  let* message_rows =
    Dream.sql request (fun db ->
      RealtimeSchema.Queries.GetMessages.collect
        db
        RealtimeSchema.Queries.GetMessages.caqti_type
        thread_id)
  in
  let latest_message_at =
    List.fold_left
      (fun latest (row : RealtimeSchema.Queries.GetMessages.row) ->
        max latest row.created_at)
      0.0
      message_rows
  in
  let messages =
    Array.map
      (fun (row : RealtimeSchema.Queries.GetMessages.row) ->
         ({ Model.Message.id = row.id; thread_id = row.thread_id; role = row.role; content = row.content }
           : Model.Message.t))
      (Array.of_list message_rows)
  in
  let* thread_rows =
    Dream.sql request (fun db ->
      RealtimeSchema.Queries.GetThreads.collect
        db
        RealtimeSchema.Queries.GetThreads.caqti_type
        ())
  in
  let threads =
    Array.map
      (fun (row : RealtimeSchema.Queries.GetThreads.row) ->
         let updated_at =
           if row.id = thread_id then max row.updated_at latest_message_at else row.updated_at
         in
         ({ Model.Thread.id = row.id; title = row.title; updated_at } : Model.Thread.t))
      (Array.of_list thread_rows)
  in
  let latest_thread_at =
    match thread_info with
    | Some thread -> thread.updated_at
    | None -> 0.0
  in
  let state : Model.t = {
    threads;
    current_thread_id = Option.map (fun (_thread : Model.Thread.t) -> thread_id) thread_info;
    messages;
    updated_at = max latest_thread_at latest_message_at;
  } in
  Lwt.return state

let get_config_json request thread_id =
  let* config = get_config request thread_id in
  Lwt.return (LlmChatStore.serializeSnapshot config)

let resolve_subscription request selection =
  match RealtimeSubscription.decode_channel selection with
  | None -> Lwt.return_none
  | Some thread_id ->
      let* thread_info =
        Dream.sql request (fun db ->
          RealtimeSchema.Queries.GetThread.find_opt
            db
            RealtimeSchema.Queries.GetThread.caqti_type
            thread_id)
      in
      (match thread_info with
       | Some _ ->
           let* () =
             Dream.sql request (fun db ->
               RealtimeSchema.Mutations.RecordThreadView.exec db thread_id)
           in
           Lwt.return_some thread_id
       | None ->
           Lwt.return_some thread_id)

let ollama_url () =
  match Sys.getenv_opt "LLM_CHAT_OLLAMA_URL" with
  | Some value when String.length value > 0 -> value
  | _ -> "http://localhost:11434/api/chat"

let ollama_model () =
  match Sys.getenv_opt "LLM_CHAT_OLLAMA_MODEL" with
  | Some value when String.length value > 0 -> value
  | _ -> "llama3.1"

let stream_event_json ~channel ~event fields =
  Yojson.Basic.to_string
    (`Assoc
      [ ("type", `String "custom");
        ("channel", `String channel);
        ("payload", `Assoc (("event", `String event) :: fields)) ])

let broadcast_stream_event ~broadcast_fn ~thread_id ~event fields =
  let message = stream_event_json ~channel:thread_id ~event fields in
  broadcast_fn thread_id (fun ~channel:_ _ -> message)

let broadcast_stream_error ~broadcast_fn ~thread_id ~assistant_message_id error =
  broadcast_stream_event ~broadcast_fn ~thread_id ~event:"stream_error"
    [ ("thread_id", `String thread_id);
      ("message_id", `String assistant_message_id);
      ("error", `String error) ]

let persist_assistant_message_content
    ~with_background_db
    ~assistant_persisted
    ~thread_id
    ~assistant_message_id
    ~content =
  if String.length content > 0 then
    with_background_db (fun db ->
      let* () =
        if !assistant_persisted then
          RealtimeSchema.Mutations.UpdateMessageContent.exec
            db
            (assistant_message_id, content)
        else
          RealtimeSchema.Mutations.AddMessage.exec
            db
            (assistant_message_id, thread_id, "assistant", content)
      in
      assistant_persisted := true;
      touch_thread_updated_at db thread_id)
  else Lwt.return_unit

let message_rows_to_json rows =
  List.map
    (fun (row : RealtimeSchema.Queries.GetMessages.row) ->
       `Assoc [("role", `String row.role); ("content", `String row.content)])
    rows

type upstream_stream_format = Unknown | Ndjson | Sse

let contains_substring ~needle value =
  try
    ignore (Str.search_forward (Str.regexp_string needle) value 0);
    true
  with Not_found -> false

let string_field name fields =
  match List.assoc_opt name fields with
  | Some (`String value) -> Some value
  | _ -> None

let assoc_field name fields =
  match List.assoc_opt name fields with
  | Some (`Assoc value) -> Some value
  | _ -> None

let list_field name fields =
  match List.assoc_opt name fields with
  | Some (`List value) -> value
  | _ -> []

let push_nonempty token tokens =
  match token with
  | Some text when String.length text > 0 -> tokens := text :: !tokens
  | _ -> ()

let token_of_message fields =
  match assoc_field "message" fields with
  | Some message_fields -> string_field "content" message_fields
  | None -> None

let token_of_delta fields =
  match assoc_field "delta" fields with
  | Some delta_fields -> (
      match string_field "content" delta_fields with
      | Some _ as token -> token
      | None -> string_field "text" delta_fields)
  | None -> string_field "delta" fields

let token_of_choice = function
  | `Assoc fields -> (
      match token_of_delta fields with
      | Some _ as token -> token
      | None -> (
          match assoc_field "message" fields with
          | Some message_fields -> string_field "content" message_fields
          | None -> string_field "text" fields))
  | _ -> None

let tokens_of_content_parts fields =
  match assoc_field "content" fields with
  | Some content_fields ->
      list_field "parts" content_fields
      |> List.filter_map (function
        | `Assoc part_fields -> string_field "text" part_fields
        | _ -> None)
  | None -> []

let tokens_of_candidate = function
  | `Assoc fields ->
      let tokens = ref [] in
      push_nonempty (string_field "text" fields) tokens;
      List.iter
        (fun text -> push_nonempty (Some text) tokens)
        (tokens_of_content_parts fields);
      List.rev !tokens
  | _ -> []

let tokens_of_json = function
  | `Assoc fields ->
      let tokens = ref [] in
      push_nonempty (string_field "response" fields) tokens;
      push_nonempty (string_field "content" fields) tokens;
      push_nonempty (string_field "text" fields) tokens;
      push_nonempty (string_field "token" fields) tokens;
      push_nonempty (token_of_message fields) tokens;
      push_nonempty (token_of_delta fields) tokens;
      List.iter
        (fun choice -> push_nonempty (token_of_choice choice) tokens)
        (list_field "choices" fields);
      List.iter
        (fun candidate ->
           List.iter
             (fun text -> push_nonempty (Some text) tokens)
             (tokens_of_candidate candidate))
        (list_field "candidates" fields);
      List.rev !tokens
  | _ -> []

let error_of_json = function
  | `Assoc fields -> (
      match List.assoc_opt "error" fields with
      | Some (`String error) -> Some error
      | Some (`Assoc error_fields) -> string_field "message" error_fields
      | _ -> None)
  | _ -> None

let is_done_json = function
  | `Assoc fields -> (
      match List.assoc_opt "done" fields with
      | Some (`Bool true) -> true
      | _ -> false)
  | _ -> false

let json_of_sse_data data =
  let trimmed = String.trim data in
  if trimmed = "" || trimmed = "[DONE]" then
    None
  else
    try Some (Yojson.Basic.from_string trimmed) with _ -> None

let looks_like_sse_chunk chunk =
  let trimmed = String.trim chunk in
  String.starts_with ~prefix:"data:" trimmed
  || String.starts_with ~prefix:"event:" trimmed
  || contains_substring ~needle:"\ndata:" chunk
  || contains_substring ~needle:"\nevent:" chunk

let content_type_is_sse resp =
  match Cohttp.Header.get (Cohttp.Response.headers resp) "content-type" with
  | Some content_type ->
      contains_substring ~needle:"text/event-stream" (String.lowercase_ascii content_type)
  | None -> false

let sse_jsons_of_chunk ~sse_buffer chunk =
  SseParser.parseChunk chunk ~buffer:sse_buffer
  |> Array.to_list
  |> List.filter_map (fun event -> json_of_sse_data event.SseParser.data)
  |> Array.of_list

let collect_upstream_jsons ~format_ref ~ndjson_parser ~sse_buffer chunk =
  match !format_ref with
  | Sse ->
      sse_jsons_of_chunk ~sse_buffer chunk
  | Ndjson ->
      NdjsonParser.feed ndjson_parser chunk
  | Unknown ->
      if looks_like_sse_chunk chunk then begin
        format_ref := Sse;
        sse_jsons_of_chunk ~sse_buffer chunk
      end else begin
        format_ref := Ndjson;
        NdjsonParser.feed ndjson_parser chunk
      end

let stream_ollama ~broadcast_fn ~with_background_db ~thread_id ~assistant_message_id ~messages_json () =
  let ollama_body =
    Yojson.Safe.to_string
      (`Assoc
        [ ("model", `String (ollama_model ()));
          ("messages", `List messages_json);
          ("stream", `Bool true) ])
  in
  let* () =
    broadcast_stream_event ~broadcast_fn ~thread_id ~event:"stream_started"
      [ ("thread_id", `String thread_id);
        ("message_id", `String assistant_message_id) ]
  in
  let* response =
    Lwt.catch
      (fun () ->
         let* response =
           Cohttp_lwt_unix.Client.post
             ~body:(Cohttp_lwt.Body.of_string ollama_body)
             ~headers:(Cohttp.Header.of_list [("Content-Type", "application/json")])
             (Uri.of_string (ollama_url ()))
         in
         Lwt.return (Ok response))
      (fun exn ->
         Lwt.return (Error (Printexc.to_string exn)))
  in
  match response with
  | Error error ->
      broadcast_stream_error ~broadcast_fn ~thread_id ~assistant_message_id error
  | Ok (resp, resp_body) ->
  let status_code = Cohttp.Response.status resp |> Cohttp.Code.code_of_status in
  if status_code < 200 || status_code >= 300 then
    let* body = Cohttp_lwt.Body.to_string resp_body in
    let error =
      Printf.sprintf "Ollama request failed with HTTP %d: %s" status_code body
    in
    broadcast_stream_error ~broadcast_fn ~thread_id ~assistant_message_id error
  else
  let body_stream = Cohttp_lwt.Body.to_stream resp_body in
  let ndjson_parser = NdjsonParser.make () in
  let sse_buffer = ref "" in
  let format_ref = ref (if content_type_is_sse resp then Sse else Unknown) in
  let all_tokens = ref [] in
  let assistant_persisted = ref false in
  let rec read_loop () =
    let* chunk = Lwt_stream.get body_stream in
    match chunk with
    | None ->
        let full_response = String.concat "" (List.rev !all_tokens) in
        if String.length full_response = 0 then
          broadcast_stream_error ~broadcast_fn ~thread_id ~assistant_message_id
            "Ollama stream completed without any response tokens"
        else
          let* () =
            persist_assistant_message_content
              ~with_background_db
              ~assistant_persisted
              ~thread_id
              ~assistant_message_id
              ~content:full_response
          in
          broadcast_stream_event ~broadcast_fn ~thread_id ~event:"stream_complete"
            [ ("thread_id", `String thread_id);
              ("message_id", `String assistant_message_id) ]
    | Some chunk ->
        (try
           let jsons =
             collect_upstream_jsons ~format_ref ~ndjson_parser ~sse_buffer chunk
           in
           let tokens = ref [] in
           let upstream_error = ref None in
           Array.iter
             (fun json ->
                match error_of_json json with
                | Some error -> upstream_error := Some error
                | None ->
                    if not (is_done_json json) then
                      List.iter
                        (fun text ->
                           tokens := text :: !tokens;
                           all_tokens := text :: !all_tokens)
                        (tokens_of_json json))
             jsons;
           match !upstream_error with
           | Some error ->
               broadcast_stream_error ~broadcast_fn ~thread_id ~assistant_message_id error
           | None ->
               let* () =
                 Lwt_list.iter_s
                   (fun text ->
                      broadcast_stream_event ~broadcast_fn ~thread_id
                        ~event:"token_received"
                        [ ("thread_id", `String thread_id);
                          ("message_id", `String assistant_message_id);
                          ("token", `String text) ])
                   (List.rev !tokens)
               in
               let current_response = String.concat "" (List.rev !all_tokens) in
               let* () =
                 persist_assistant_message_content
                   ~with_background_db
                   ~assistant_persisted
                   ~thread_id
                   ~assistant_message_id
                   ~content:current_response
               in
               read_loop ()
         with exn ->
           broadcast_stream_error ~broadcast_fn ~thread_id ~assistant_message_id
             (Printexc.to_string exn))
  in
  read_loop ()

let require_thread db thread_id f =
  let* thread =
    RealtimeSchema.Queries.GetThread.find_opt
      db
      RealtimeSchema.Queries.GetThread.caqti_type
      thread_id
  in
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
             Some
               (LlmChatStore.SendPrompt
                  { LlmChatStore.message_id = ""; assistant_message_id = ""; thread_id = ""; prompt = "" })
         | "delete_thread" -> Some (LlmChatStore.DeleteThread "")
         | "select_thread" -> Some (LlmChatStore.SelectThread "")
         | "create_new_thread" ->
             Some (LlmChatStore.CreateNewThread { LlmChatStore.id = ""; LlmChatStore.title = "" })
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
                 let* thread =
                   Dream.sql request (fun db ->
                     RealtimeSchema.Queries.GetThread.find_opt
                       db
                       RealtimeSchema.Queries.GetThread.caqti_type
                       thread_id)
                 in
                 Lwt.return (match thread with Some _ -> Some thread_id | None -> None)
             | None -> Lwt.return_none
           in
           let state : Model.t = {
             threads = [||];
             current_thread_id;
             messages = [||];
             updated_at = 0.0;
           } in
           (match
              StoreBuilder.GuardTree.resolve ~state ~action LlmChatStore.guardTree
            with
            | StoreRuntimeTypes.Allow -> Lwt.return (Ok ())
            | StoreRuntimeTypes.Deny reason -> Lwt.return (Error reason)))
    (function
      | Caqti_error.Exn error -> Lwt.return (Error (Caqti_error.show error))
      | exn -> Lwt.return (Error (Printexc.to_string exn)))

let handle_mutation with_background_db broadcast_fn request ~db ~action_id ~mutation_name:_ action =
  let open Mutation_json in
  let kind =
    match assoc "kind" action with
    | Some (`String value) -> Ok value
    | _ -> Error "Missing action kind"
  in
  match kind with
  | Error error -> Lwt.return (Ack (Error error))
  | Ok "send_prompt" ->
      begin
        match assoc "payload" action with
        | None -> Lwt.return (Ack (Error "Missing send_prompt payload"))
        | Some payload ->
            begin
              match required_string "thread_id" payload with
              | Error error -> Lwt.return (Ack (Error error))
              | Ok thread_id ->
                  begin
                    match required_string "prompt" payload with
                    | Error error -> Lwt.return (Ack (Error error))
                    | Ok prompt ->
                        require_thread db thread_id (fun () ->
                          let message_id =
                            match required_string "message_id" payload with
                            | Ok id -> id
                            | Error _ -> UUID.make ()
                          in
                          let assistant_message_id =
                            match required_string "assistant_message_id" payload with
                            | Ok id -> id
                            | Error _ -> UUID.make ()
                          in
                          Lwt.catch
                            (fun () ->
                               let* () =
                                 RealtimeSchema.Mutations.AddMessage.exec
                                   db
                                   (message_id, thread_id, "user", prompt)
                               in
                               let* () = touch_thread_updated_at db thread_id in
                               let* message_rows =
                                 RealtimeSchema.Queries.GetMessages.collect
                                   db
                                   RealtimeSchema.Queries.GetMessages.caqti_type
                                   thread_id
                               in
                               let messages_json = message_rows_to_json message_rows in
                               let () =
                                 Lwt.async (fun () ->
                                   Lwt.catch
                                     (fun () ->
                                        stream_ollama ~broadcast_fn ~with_background_db ~thread_id
                                          ~assistant_message_id ~messages_json ())
                                     (fun exn ->
                                        broadcast_stream_error ~broadcast_fn ~thread_id
                                          ~assistant_message_id (Printexc.to_string exn)))
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
      begin
        match assoc "payload" action with
        | None -> Lwt.return (Ack (Error "Missing create_new_thread payload"))
        | Some payload ->
            begin
              match (required_string "id" payload, required_string "title" payload) with
              | (Ok thread_id, Ok title) ->
                  let* result = RealtimeSchema.Mutations.CreateThread.exec db (thread_id, title) in
                  (match result with
                   | () ->
                       let* () = RealtimeSchema.Mutations.RecordThreadView.exec db thread_id in
                       Lwt.return (Ack (Ok ()))
                   | exception Caqti_error.Exn error ->
                       Lwt.return (Ack (Error (Caqti_error.show error)))
                   | exception exn ->
                       Lwt.return (Ack (Error (Printexc.to_string exn))))
              | (Error error, _) | (_, Error error) ->
                  Lwt.return (Ack (Error error))
            end
      end
  | Ok "delete_thread" ->
      begin
        match assoc "payload" action with
        | None -> Lwt.return (Ack (Error "Missing delete_thread payload"))
        | Some payload ->
            begin
              match required_string "thread_id" payload with
              | Error error -> Lwt.return (Ack (Error error))
              | Ok thread_id ->
                  Lwt.catch
                    (fun () ->
                       let* () = RealtimeSchema.Mutations.DeleteThread.exec db thread_id in
                       Lwt.return (Ack (Ok ())))
                    (function
                      | Caqti_error.Exn error ->
                          Lwt.return (Ack (Error (Caqti_error.show error)))
                      | exn ->
                          Lwt.return (Ack (Error (Printexc.to_string exn))))
            end
      end
  | Ok "select_thread" ->
      Lwt.return (Ack (Ok ()))
  | Ok _ -> Lwt.return (Ack (Error "Unknown action kind"))

let dispatch_mutation db ~mutation_name action =
  match mutation_name with
  | "delete_thread" -> None
  | _ -> RealtimeSchema.dispatch_mutation db ~mutation_name action

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
  let background_pool =
    match Caqti_lwt_unix.connect_pool (Uri.of_string db_uri) with
    | Ok pool -> pool
    | Error error -> failwith (Caqti_error.show error)
  in
  let with_background_db f =
    let* result =
      Caqti_lwt_unix.Pool.use
        (fun db ->
           let* () = f db in
           Lwt.return (Ok ()))
        background_pool
    in
    Caqti_lwt.or_fail result
  in
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
    ~dispatch_mutation
    ~handle_mutation:(handle_mutation with_background_db)
    ~validate_mutation
  |> Server_builder.with_routes [
    Dream.get "/static/**" (Dream.static doc_root);
    Dream.get "/app.js" (fun req -> Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req -> Dream.from_filesystem doc_root "Index.re.css" req);
    Dream.get "/favicon.ico" (fun _ -> Dream.respond ~status:`No_Content "");
    Dream.get "/" (fun req ->
      let* thread_rows = Dream.sql req (fun db ->
        RealtimeSchema.Queries.GetThreads.collect
          db
          RealtimeSchema.Queries.GetThreads.caqti_type
          ())
      in
      let threads = Array.of_list thread_rows in
      match Array.length threads > 0 with
      | true -> Dream.redirect req ("/" ^ threads.(0).id)
      | false ->
          let uuid = UUID.make () in
          let* _ = Dream.sql req (fun db ->
            RealtimeSchema.Mutations.CreateThread.exec db (uuid, "New Chat")) in
          Dream.redirect req ("/" ^ uuid));
    Dream.post "/api/test/delete-thread/:thread_id" (fun request ->
      let thread_id = Dream.param request "thread_id" in
      let* _ = Dream.sql request (fun db ->
        RealtimeSchema.Mutations.DeleteThread.exec db thread_id) in
      Dream.json "{\"status\":\"deleted\"}");
    Dream.post "/api/test/create-thread" (fun request ->
      let* body = Dream.body request in
      let json = Yojson.Safe.from_string body in
      let title = try Yojson.Safe.Util.(to_string (member "title" json)) with _ -> "Test Thread" in
      let uuid = UUID.make () in
      let* _ = Dream.sql request (fun db ->
        RealtimeSchema.Mutations.CreateThread.exec db (uuid, title)) in
      Dream.respond (Yojson.Safe.to_string (`Assoc [("id", `String uuid); ("title", `String title)])));
    Dream.post "/api/test/add-message" (fun request ->
      let* body = Dream.body request in
      let json = Yojson.Safe.from_string body in
      let thread_id = try Yojson.Safe.Util.(to_string (member "thread_id" json)) with _ -> "" in
      let role = try Yojson.Safe.Util.(to_string (member "role" json)) with _ -> "assistant" in
      let content = try Yojson.Safe.Util.(to_string (member "content" json)) with _ -> "test message" in
      let message_id = UUID.make () in
      let* _ = Dream.sql request (fun db ->
        let* () =
          RealtimeSchema.Mutations.AddMessage.exec db (message_id, thread_id, role, content)
        in
        touch_thread_updated_at db thread_id) in
      Dream.respond (Yojson.Safe.to_string (`Assoc [("id", `String message_id); ("thread_id", `String thread_id)])));
    Dream.post "/api/test/delete-all-threads" (fun request ->
      let* _ =
        Dream.sql request (fun db ->
          let* thread_ids = collect_thread_ids db in
          let* active_thread_ids = collect_active_thread_view_ids db in
          let channels = thread_ids @ active_thread_ids in
          let* () = RealtimeSchema.Mutations.DeleteAllThreads.exec db () in
          notify_deleted_threads db ~channels ~thread_ids)
      in
      Dream.json "{\"status\":\"deleted_all\"}");
    Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
  ]
  |> Server_builder.run
