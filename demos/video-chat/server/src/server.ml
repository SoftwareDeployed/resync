open Lwt.Syntax
open Mutation_result

let adapter = VideoChat_adapter.create ()

let resolve_subscription _request selection =
  Lwt.return (Some selection)

let load_snapshot _request channel =
  let room_json =
    match VideoChat_adapter.get_room adapter ~peer_id:channel with
    | Some room_id ->
      let peers = VideoChat_adapter.get_peers adapter ~room_id in
      let peers_json = List.map (fun peer_id ->
        `Assoc [("id", `String peer_id); ("joined_at", `Float (Unix.gettimeofday ()))])
      peers in
      `Assoc [
        ("id", `String room_id);
        ("created_at", `Float (Unix.gettimeofday ()));
        ("peers", `List peers_json);
      ]
    | None -> `Null
  in
  let messages =
    match VideoChat_adapter.get_room adapter ~peer_id:channel with
    | Some room_id -> VideoChat_adapter.get_room_messages adapter ~room_id
    | None -> [||]
  in
  let messages_json = Array.map (fun (msg: VideoChat_adapter.chat_message) ->
    `Assoc [
      ("id", `String msg.id);
      ("sender_id", `String msg.sender_id);
      ("text", `String msg.text);
      ("sent_at", `Float msg.sent_at);
    ]
  ) messages in
  let json =
    `Assoc [
      ("client_id", `String channel);
      ("room", room_json);
      ("is_joined", `Bool false);
      ("local_video_enabled", `Bool true);
      ("local_audio_enabled", `Bool true);
      ("remote_peer_id", `Null);
      ("remote_video_enabled", `Bool true);
      ("remote_audio_enabled", `Bool true);
      ("messages", `List (Array.to_list messages_json));
      ("updated_at", `Float (Unix.gettimeofday ()));
    ] |> Yojson.Basic.to_string
  in
  Lwt.return json

let assoc key json =
  match json with
  | `Assoc fields -> (try Some (List.assoc key fields) with Not_found -> None)
  | _ -> None

let get_string key json =
  match assoc key json with Some (`String v) -> Some v | _ -> None

let get_bool key json =
  match assoc key json with Some (`Bool v) -> Some v | _ -> None

let patch_json kind payload =
  `Assoc [("kind", `String kind); ("payload", payload)] |> Yojson.Basic.to_string

let wrap_message msg =
  match Yojson.Basic.from_string msg with
  | `Assoc fields ->
    (try
      let type_field = List.assoc "type" fields in
      match type_field with
      | `String "media" -> msg
      | _ ->
        `Assoc [
          ("type", `String "patch");
          ("timestamp", `Float (Unix.gettimeofday () *. 1000.0));
          ("payload", `Assoc fields);
        ] |> Yojson.Basic.to_string
      with Not_found ->
        `Assoc [
          ("type", `String "patch");
          ("timestamp", `Float (Unix.gettimeofday () *. 1000.0));
          ("payload", `Assoc fields);
        ] |> Yojson.Basic.to_string)
  | _ -> msg

let broadcast_to_peers broadcast_fn peers except_id msg =
  Lwt_list.iter_s (fun peer_id ->
    if peer_id <> except_id then
      broadcast_fn peer_id (fun ~channel:_ _ -> wrap_message msg)
    else
      Lwt.return_unit
  ) peers

let broadcast_fns : (string, (string -> (channel:string -> string -> string) -> unit Lwt.t)) Hashtbl.t = Hashtbl.create 16

let handle_mutation broadcast_fn _request ~db:_ ~action_id ~mutation_name:_ action =
  match get_string "kind" action with

  | Some "join_room" ->
    (match assoc "payload" action with
    | Some payload ->
      (match (get_string "room_id" payload, get_string "peer_id" payload) with
      | Some room_id, Some peer_id ->
        let existing_peers = VideoChat_adapter.get_peers adapter ~room_id in
        if List.length existing_peers >= 2 then
          Lwt.return (Ack (Error "Room is full (max 2 peers)"))
        else begin
          Hashtbl.replace broadcast_fns peer_id broadcast_fn;
          VideoChat_adapter.add_peer adapter ~room_id ~peer_id;
          let join_msg = patch_json "peer_joined"
            (`Assoc [("room_id", `String room_id); ("peer_id", `String peer_id); ("joined_at", `Float (Unix.gettimeofday ()))]) in
          let* () = broadcast_to_peers broadcast_fn existing_peers peer_id join_msg in
          let all_peers = peer_id :: existing_peers in
          let* () = Lwt_list.iter_s (fun existing_peer_id ->
            let peer_info_msg = patch_json "peer_joined"
              (`Assoc [("room_id", `String room_id); ("peer_id", `String existing_peer_id); ("joined_at", `Float (Unix.gettimeofday ()))]) in
            broadcast_fn peer_id (fun ~channel:_ _ -> wrap_message peer_info_msg)
          ) all_peers in
          Lwt.return (Ack (Ok ()))
        end
      | _ -> Lwt.return (Ack (Error "Missing room_id or peer_id")))
    | None -> Lwt.return (Ack (Error "Missing payload")))
  | Some "leave_room" ->
    (match assoc "payload" action with
    | Some payload ->
      (match (get_string "room_id" payload, get_string "peer_id" payload) with
      | Some room_id, Some peer_id ->
        VideoChat_adapter.remove_peer adapter ~room_id ~peer_id;
        let peers = VideoChat_adapter.get_peers adapter ~room_id in
        let left_msg = patch_json "peer_left"
        (`Assoc [("room_id", `String room_id); ("peer_id", `String peer_id)]) in
        let* () = broadcast_to_peers broadcast_fn peers "" left_msg in
        Lwt.return (Ack (Ok ()))
      | _ -> Lwt.return (Ack (Ok ())))
    | None -> Lwt.return (Ack (Ok ())))
  | Some "toggle_video" ->
    (match assoc "payload" action with
    | Some payload ->
      (match (get_string "room_id" payload, get_string "peer_id" payload, get_bool "enabled" payload) with
      | Some room_id, Some peer_id, Some enabled ->
        let msg = patch_json "toggle_video"
          (`Assoc [("room_id", `String room_id); ("peer_id", `String peer_id); ("enabled", `Bool enabled)]) in
        let peers = VideoChat_adapter.get_peers adapter ~room_id in
        let* () = broadcast_to_peers broadcast_fn peers peer_id msg in
        Lwt.return (Ack (Ok ()))
      | _ -> Lwt.return (Ack (Ok ())))
    | None -> Lwt.return (Ack (Ok ())))
  | Some "toggle_audio" ->
    (match assoc "payload" action with
    | Some payload ->
      (match (get_string "room_id" payload, get_string "peer_id" payload, get_bool "enabled" payload) with
      | Some room_id, Some peer_id, Some enabled ->
        let msg = patch_json "toggle_audio"
          (`Assoc [("room_id", `String room_id); ("peer_id", `String peer_id); ("enabled", `Bool enabled)]) in
        let peers = VideoChat_adapter.get_peers adapter ~room_id in
        let* () = broadcast_to_peers broadcast_fn peers peer_id msg in
        Lwt.return (Ack (Ok ()))
      | _ -> Lwt.return (Ack (Ok ())))
    | None -> Lwt.return (Ack (Ok ())))
  | Some "send_message" ->
    (match assoc "payload" action with
    | Some payload ->
      (match (get_string "room_id" payload, get_string "peer_id" payload, get_string "text" payload) with
      | Some room_id, Some peer_id, Some text ->
        (match VideoChat_adapter.get_room adapter ~peer_id with
        | Some peer_room_id when peer_room_id = room_id ->
          (match VideoChat_adapter.add_message adapter ~room_id ~sender_id:peer_id ~text with
          | Some msg ->
            let chat_patch = `Assoc [
              ("type", `String "patch");
              ("table", `String "chat_messages");
              ("action", `String "INSERT");
              ("data", `Assoc [
                ("id", `String msg.id);
                ("sender_id", `String msg.sender_id);
                ("text", `String msg.text);
                ("sent_at", `Float msg.sent_at)
              ]);
            ] |> Yojson.Basic.to_string in
            let peers = VideoChat_adapter.get_peers adapter ~room_id in
            let* () = broadcast_to_peers broadcast_fn peers peer_id chat_patch in
            Lwt.return (Ack (Ok ()))
          | None -> Lwt.return (Ack (Error "Failed to add message")))
        | _ -> Lwt.return (Ack (Error "Peer not in room")))
      | _ -> Lwt.return (Ack (Error "Missing room_id, peer_id, or text")))
    | None -> Lwt.return (Ack (Error "Missing payload")))
  | Some "noop" ->
    Lwt.return (Ack (Ok ()))
  | _ -> Lwt.return (Ack (Error "Unknown action kind"))

let handle_media broadcast_fn _request channel payload_str =
  Hashtbl.replace broadcast_fns channel broadcast_fn;

  let payload = Yojson.Basic.from_string payload_str in
  match (get_string "room_id" payload, get_string "peer_id" payload) with
  | Some room_id, Some peer_id ->
    (match VideoChat_adapter.get_room adapter ~peer_id with
    | Some peer_room_id when peer_room_id = room_id ->
      let media_msg = `Assoc [ ("type", `String "media"); ("payload", payload) ] |> Yojson.Basic.to_string in
      VideoChat_adapter.queue_media_frame adapter ~room_id ~except_id:peer_id media_msg;
      Lwt.return (Ok ())
    | _ ->
      Printf.eprintf "[handle_media] rejecting media from peer %s - not in room %s\n%!" peer_id room_id;
      let error_msg = `Assoc [
        ("type", `String "error");
        ("message", `String ("Peer " ^ peer_id ^ " is not in room " ^ room_id));
      ] |> Yojson.Basic.to_string in
      let* () = broadcast_fn peer_id (fun ~channel:_ _ -> error_msg) in
      Lwt.return (Error ("Peer " ^ peer_id ^ " is not in room " ^ room_id)))
  | _ -> Lwt.return (Error "Missing room_id or peer_id")

let rec media_polling_loop () =
  let* () = Lwt_unix.sleep 0.001 in
  let all_peers = VideoChat_adapter.get_all_peer_ids adapter in
  let* () = Lwt_list.iter_s (fun peer_id ->
    match VideoChat_adapter.get_next_frame adapter ~peer_id:peer_id with
    | Some frame_data ->
        (match Hashtbl.find_opt broadcast_fns peer_id with
        | Some broadcast_fn ->
            broadcast_fn peer_id (fun ~channel:_ _ -> frame_data)
        | None -> Lwt.return_unit)
    | None -> Lwt.return_unit
  ) all_peers in
  media_polling_loop ()

let handle_disconnect broadcast_fn channel =
  Printf.eprintf "[handle_disconnect] called for channel=%s\n%!" channel;
  Hashtbl.remove broadcast_fns channel;
  match VideoChat_adapter.get_room adapter ~peer_id:channel with
  | Some room_id ->
    Printf.eprintf "[handle_disconnect] peer %s was in room %s\n%!" channel room_id;
    VideoChat_adapter.remove_peer adapter ~room_id ~peer_id:channel;
    let peers = VideoChat_adapter.get_peers adapter ~room_id in
    Printf.eprintf "[handle_disconnect] room has %d remaining peers\n%!" (List.length peers);
    if List.length peers > 0 then begin
      Printf.eprintf "[handle_disconnect] preparing peer_left message\n%!";
      let left_msg = patch_json "peer_left"
        (`Assoc [("room_id", `String room_id); ("peer_id", `String channel)]) in
      Printf.eprintf "[handle_disconnect] calling broadcast_to_peers\n%!";
      let* () = broadcast_to_peers broadcast_fn peers "" left_msg in
      Printf.eprintf "[handle_disconnect] broadcast_to_peers completed\n%!";
      Lwt.return_unit
    end else
      Lwt.return_unit
  | None ->
    Printf.eprintf "[handle_disconnect] peer %s was not in a room\n%!" channel;
    Lwt.return_unit

let () =
  let builder =
    Server_builder.make
      ~doc_root:(match Sys.getenv_opt "VIDEO_CHAT_DOC_ROOT" with
                 | Some d -> d
                 | None -> "./_build/default/demos/video-chat/ui/src/")
      ~interface_var:"SERVER_INTERFACE"
      ~port_var:"VIDEO_CHAT_SERVER_PORT"
      ~default_interface:"127.0.0.1"
      ~default_port:8897
      ()
  in
  let doc_root = Server_builder.doc_root builder in
  builder
  |> Server_builder.with_packed_adapter
       (Adapter.pack
          (module VideoChat_adapter : Adapter.S with type t = VideoChat_adapter.t)
          adapter)
  |> Server_builder.with_middleware
    ~resolve_subscription
    ~load_snapshot
    ~handle_mutation
    ~handle_media
    ~handle_disconnect
  |> Server_builder.with_pre_start (fun () -> Lwt.async media_polling_loop)
  |> Server_builder.with_routes [
    Dream.get "/" (fun _ ->
      Dream.html "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Video Chat</title><link rel='stylesheet' href='/style.css'></head><body><div id='root'></div><script type='module' src='/app.js'></script></body></html>");
    Dream.get "/room/**" (fun _ ->
      Dream.html "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Video Chat</title><link rel='stylesheet' href='/style.css'></head><body><div id='root'></div><script type='module' src='/app.js'></script></body></html>");
    Dream.get "/app.js" (fun req -> Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req -> Dream.from_filesystem doc_root "Index.re.css" req);
  ]
  |> Server_builder.run
