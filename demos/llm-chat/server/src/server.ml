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

let handle_mutation _broadcast_fn request ~action_id action =
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
                let* result =
                  Dream.sql request
                    (Database.Chat.add_message (action_id, message_id, thread_id, "user", prompt))
                in
                (match result with
                 | () -> Lwt.return (Middleware.Ack (Ok ()))
                 | exception Caqti_error.Exn error ->
                     Lwt.return (Middleware.Ack (Error (Caqti_error.show error)))
                 | exception exn ->
                     Lwt.return (Middleware.Ack (Error (Printexc.to_string exn))))
             | Error error, _ | _, Error error ->
                 Lwt.return (Middleware.Ack (Error error)))
        | None -> Lwt.return (Middleware.Ack (Error "Missing send_prompt payload")))
  | Ok "append_token"
  | Ok "finish_response"
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
           Dream.post "/api/chat" (fun request ->
             let* body = Dream.body request in
             let json = Yojson.Safe.from_string body in
             let thread_id =
               try Yojson.Safe.Util.(to_string (member "thread_id" json))
               with _ -> ""
             in
             let messages =
               try
                 let items = Yojson.Safe.Util.(to_list (member "messages" json)) in
                 List.filter_map (fun item ->
                   try
                     let role = Yojson.Safe.Util.(to_string (member "role" item)) in
                     let content = Yojson.Safe.Util.(to_string (member "content" item)) in
                     Some (`Assoc [("role", `String role); ("content", `String content)])
                   with _ -> None
                 ) items
               with _ -> []
             in
             Dream.stream ~headers:[
               ("Content-Type", "text/event-stream");
               ("Cache-Control", "no-cache");
               ("X-Accel-Buffering", "no");
             ] (fun stream ->
                let ollama_body = Yojson.Safe.to_string (`Assoc [
                 ("model", `String "llama3.1");
                 ("messages", `List messages);
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
                     let error_data = Yojson.Safe.to_string (`Assoc [
                       ("type", `String "error");
                       ("message", `String ("Ollama connection failed: " ^ Printexc.to_string exn))
                     ]) in
                     let* () = SseWriter.writeEvent stream ~data:error_data () in
                     Lwt.fail exn)
               in

               let body_stream = Cohttp_lwt.Body.to_stream resp_body in
               let ndjson_parser = NdjsonParser.make () in
               let wrapper_stream, wrapper_push = Lwt_stream.create () in
               let all_tokens = ref [] in

               let rec read_loop () =
                 let* chunk = Lwt_stream.get body_stream in
                 match chunk with
                 | None ->
                     wrapper_push None;
                     Lwt.return_unit
                 | Some chunk ->
                     (try
                        let jsons = NdjsonParser.feed ndjson_parser chunk in
                        Array.iter (fun json ->
                          match json with
                          | `Assoc fields ->
                              let done_field = List.assoc_opt "done" fields in
                              (match done_field with
                               | Some (`Bool true) -> ()
                               | _ ->
                                 match List.assoc_opt "message" fields with
                                 | Some (`Assoc msg_fields) ->
                                     (match List.assoc_opt "content" msg_fields with
                                      | Some (`String text) when String.length text > 0 ->
                                          all_tokens := text :: !all_tokens;
                                          wrapper_push (Some text)
                                      | _ -> ())
                                 | _ -> ())
                          | _ -> ()
                        ) jsons
                      with _ -> ());
                     read_loop ()
               in
               Lwt.async read_loop;
               
               let dp = StreamPipeDream.fromLwtStream(wrapper_stream) in
               let* () = StreamPipeDream.broadcast_with_lwt dp ~send:(fun token ->
                 let data = Yojson.Safe.to_string (`Assoc [
                   ("type", `String "token");
                   ("content", `String token)
                 ]) in
                 SseWriter.writeEvent stream ~data ()
               ) in
               
               let* () = SseWriter.writeEvent stream ~data:(Yojson.Safe.to_string (`Assoc [("type", `String "done")])) () in
               
               let full_response = String.concat "" (List.rev !all_tokens) in
               if String.length full_response > 0 then
                 let message_id = UUID.make () in
                 let action_id = UUID.make () in
                 let* _ = Dream.sql request
                   (Database.Chat.add_message (action_id, message_id, thread_id, "assistant", full_response))
                 in
                 Lwt.return_unit
               else
                 Lwt.return_unit
             ));
          Dream.get "/" (fun req ->
             let uuid = UUID.make () in
              let* _ = Dream.sql req (Database.Chat.create_thread uuid "New Chat") in
             Dream.redirect req ("/" ^ uuid));
         Dream.get "/threads" (fun req ->
             let* threads = Dream.sql req (Database.Chat.get_threads ()) in
             let threads_json =
               `List (
                 Array.to_list threads
                 |> List.map (fun (thread: Model.Thread.t) ->
                   `Assoc [
                     ("id", `String thread.id);
                     ("title", `String thread.title);
                     ("updated_at", `Float thread.updated_at)
                   ])
               )
             in
             Dream.json (Yojson.Basic.to_string threads_json));
         Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app) ]
