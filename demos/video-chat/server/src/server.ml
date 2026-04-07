open Lwt.Syntax

let doc_root =
  match Sys.getenv_opt "VIDEO_CHAT_DOC_ROOT" with
  | Some doc_root -> doc_root
  | None -> "./_build/default/demos/video-chat/ui/src/"

let server_interface =
  match Sys.getenv_opt "SERVER_INTERFACE" with
  | Some interface -> interface
  | None -> "127.0.0.1"

let server_port =
  match Sys.getenv_opt "VIDEO_CHAT_SERVER_PORT" with
  | Some port -> int_of_string port
  | None -> 8897

let adapter = VideoChat_adapter.create ()

let resolve_subscription _request selection =
  Lwt.return (Some selection)

let load_snapshot _request channel =
  (* Check if this peer is already in a room *)
  let room_json =
    match VideoChat_adapter.get_room adapter ~peer_id:channel with
    | Some room_id ->
      (* Peer is in a room, return room with existing peers *)
      let peers = VideoChat_adapter.get_peers adapter ~room_id in
      let peers_json = List.map (fun peer_id ->
        `Assoc [("id", `String peer_id); ("joined_at", `Float (Unix.gettimeofday ()))]
      ) peers in
      `Assoc [
        ("id", `String room_id);
        ("created_at", `Float (Unix.gettimeofday ()));
        ("peers", `List peers_json);
      ]
    | None -> `Null
  in
  let json =
    `Assoc [
      ("client_id", `String channel);
      ("room", room_json);
      ("local_video_enabled", `Bool true);
      ("local_audio_enabled", `Bool true);
      ("remote_peer_id", `Null);
      ("remote_video_enabled", `Bool true);
      ("remote_audio_enabled", `Bool true);
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
      | `String "media" -> msg (* Send media messages as-is *)
      | _ ->
        (* For non-media messages, wrap as patch with timestamp *)
        `Assoc [
          ("type", `String "patch");
          ("timestamp", `Float (Unix.gettimeofday () *. 1000.0));
          ("payload", `Assoc fields);
        ] |> Yojson.Basic.to_string
      with Not_found ->
        (* No type field, wrap as patch *)
        `Assoc [
          ("type", `String "patch");
          ("timestamp", `Float (Unix.gettimeofday () *. 1000.0));
          ("payload", `Assoc fields);
        ] |> Yojson.Basic.to_string
    )
  | _ -> msg

let broadcast_to_peers broadcast_fn peers except_id msg =
  Lwt_list.iter_s (fun peer_id ->
    if peer_id <> except_id then
      broadcast_fn peer_id (fun _ -> wrap_message msg)
    else
      Lwt.return_unit
  ) peers

(* Store broadcast functions for each channel to enable media polling *)
let broadcast_fns : (string, (string -> (string -> string) -> unit Lwt.t)) Hashtbl.t = Hashtbl.create 16

let handle_mutation broadcast_fn _request ~action_id action =
  match get_string "kind" action with

  | Some "join_room" ->
    (match assoc "payload" action with
    | Some payload ->
      (match (get_string "room_id" payload, get_string "peer_id" payload) with
      | Some room_id, Some peer_id ->
        (* Check if room already has 2 peers (limit for demo) *)
        let existing_peers = VideoChat_adapter.get_peers adapter ~room_id in
        if List.length existing_peers >= 2 then
          Lwt.return (Middleware.Ack (Error "Room is full (max 2 peers)"))
        else begin
          (* Store broadcast_fn for this peer so they can receive media *)
          Hashtbl.replace broadcast_fns peer_id broadcast_fn;
          (* Add peer to room *)
          VideoChat_adapter.add_peer adapter ~room_id ~peer_id;
          (* Notify existing peers about the new peer *)
          let join_msg = patch_json "peer_joined"
            (`Assoc [("room_id", `String room_id); ("peer_id", `String peer_id); ("joined_at", `Float (Unix.gettimeofday ()))]) in
          let* () = broadcast_to_peers broadcast_fn existing_peers peer_id join_msg in
          (* Notify new peer about ALL existing peers (including themselves) *)
          let all_peers = peer_id :: existing_peers in
          let* () = Lwt_list.iter_s (fun existing_peer_id ->
            let peer_info_msg = patch_json "peer_joined"
              (`Assoc [("room_id", `String room_id); ("peer_id", `String existing_peer_id); ("joined_at", `Float (Unix.gettimeofday ()))]) in
            broadcast_fn peer_id (fun _ -> wrap_message peer_info_msg)
          ) all_peers in
          Lwt.return (Middleware.Ack (Ok ()))
        end
      | _ -> Lwt.return (Middleware.Ack (Error "Missing room_id or peer_id")))
    | None -> Lwt.return (Middleware.Ack (Error "Missing payload")))
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
        Lwt.return (Middleware.Ack (Ok ()))
      | _ -> Lwt.return (Middleware.Ack (Ok ())))
    | None -> Lwt.return (Middleware.Ack (Ok ())))
  | Some "toggle_video" ->
    (match assoc "payload" action with
    | Some payload ->
      (match (get_string "room_id" payload, get_string "peer_id" payload, get_bool "enabled" payload) with
      | Some room_id, Some peer_id, Some enabled ->
        let msg = patch_json "toggle_video"
          (`Assoc [("room_id", `String room_id); ("peer_id", `String peer_id); ("enabled", `Bool enabled)]) in
        let peers = VideoChat_adapter.get_peers adapter ~room_id in
        let* () = broadcast_to_peers broadcast_fn peers peer_id msg in
        Lwt.return (Middleware.Ack (Ok ()))
      | _ -> Lwt.return (Middleware.Ack (Ok ())))
    | None -> Lwt.return (Middleware.Ack (Ok ())))
  | Some "toggle_audio" ->
    (match assoc "payload" action with
    | Some payload ->
      (match (get_string "room_id" payload, get_string "peer_id" payload, get_bool "enabled" payload) with
      | Some room_id, Some peer_id, Some enabled ->
        let msg = patch_json "toggle_audio"
          (`Assoc [("room_id", `String room_id); ("peer_id", `String peer_id); ("enabled", `Bool enabled)]) in
        let peers = VideoChat_adapter.get_peers adapter ~room_id in
        let* () = broadcast_to_peers broadcast_fn peers peer_id msg in
        Lwt.return (Middleware.Ack (Ok ()))
      | _ -> Lwt.return (Middleware.Ack (Ok ())))
    | None -> Lwt.return (Middleware.Ack (Ok ())))
  | Some "noop" ->
    Lwt.return (Middleware.Ack (Ok ()))
  | _ -> Lwt.return (Middleware.Ack (Error "Unknown action kind"))
let handle_media broadcast_fn _request channel payload_str =
  (* Store broadcast_fn for this channel so polling loop can use it *)
  Hashtbl.replace broadcast_fns channel broadcast_fn;

  let payload = Yojson.Basic.from_string payload_str in
  match (get_string "room_id" payload, get_string "peer_id" payload) with
  | Some room_id, Some peer_id ->
    (* Queue frame for each peer (drops old frames if queue full) *)
    let media_msg = `Assoc [ ("type", `String "media"); ("payload", payload) ] |> Yojson.Basic.to_string in
    VideoChat_adapter.queue_media_frame adapter ~room_id ~except_id:peer_id media_msg;
    Lwt.return_unit
  | _ -> Lwt.return_unit

(* Polling loop that sends queued media frames *)
let rec media_polling_loop () =
  let* () = Lwt_unix.sleep 0.001 in  (* 1ms poll interval *)
  let all_peers = VideoChat_adapter.get_all_peer_ids adapter in
  let* () = Lwt_list.iter_s (fun peer_id ->
    match VideoChat_adapter.get_next_frame adapter ~peer_id:peer_id with
    | Some frame_data ->
        (match Hashtbl.find_opt broadcast_fns peer_id with
        | Some broadcast_fn ->
            (* Send frame directly to this peer *)
            broadcast_fn peer_id (fun _ -> frame_data)
        | None -> Lwt.return_unit)
    | None -> Lwt.return_unit
  ) all_peers in
  media_polling_loop ()

let handle_disconnect broadcast_fn channel =
  Printf.eprintf "[handle_disconnect] called for channel=%s\n%!" channel;
  (* Remove broadcast_fn for this channel *)
  Hashtbl.remove broadcast_fns channel;
  (* Check if this peer was in a room *)
  match VideoChat_adapter.get_room adapter ~peer_id:channel with
  | Some room_id ->
    Printf.eprintf "[handle_disconnect] peer %s was in room %s\n%!" channel room_id;
    (* Remove peer from room *)
    VideoChat_adapter.remove_peer adapter ~room_id ~peer_id:channel;
    (* Notify other peers in the room *)
    let peers = VideoChat_adapter.get_peers adapter ~room_id in
    Printf.eprintf "[handle_disconnect] room has %d remaining peers\n%!" (List.length peers);
    if List.length peers > 0 then begin
      Printf.eprintf "[handle_disconnect] preparing peer_left message\n%!" ;
      let left_msg = patch_json "peer_left"
        (`Assoc [("room_id", `String room_id); ("peer_id", `String channel)]) in
      Printf.eprintf "[handle_disconnect] calling broadcast_to_peers\n%!" ;
      let* () = broadcast_to_peers broadcast_fn peers "" left_msg in
      Printf.eprintf "[handle_disconnect] broadcast_to_peers completed\n%!" ;
      Lwt.return_unit
    end else
      Lwt.return_unit
  | None ->
    Printf.eprintf "[handle_disconnect] peer %s was not in a room\n%!" channel;
    Lwt.return_unit

let realtime_adapter =
  Adapter.pack
    (module VideoChat_adapter : Adapter.S with type t = VideoChat_adapter.t)
    adapter

let realtime_middleware =
  Middleware.create ~adapter:realtime_adapter ~resolve_subscription
    ~load_snapshot ~handle_mutation ~handle_media ~handle_disconnect ()

let () =
  Lwt.async media_polling_loop;  (* Start media frame polling *)
  Printf.eprintf "[server] starting on %s:%d\n%!" server_interface server_port;
  Dream.run ~interface:server_interface ~port:server_port
  @@ Dream.logger
  @@ Dream.router [
    Middleware.route "_events" realtime_middleware;
    Dream.get "/" (fun _ ->
      Dream.html "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Video Chat</title><link rel='stylesheet' href='/style.css'></head><body><div id='root'></div><script type='module' src='/app.js'></script></body></html>");
    Dream.get "/room/**" (fun _ ->
      Dream.html "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Video Chat</title><link rel='stylesheet' href='/style.css'></head><body><div id='root'></div><script type='module' src='/app.js'></script></body></html>");
    Dream.get "/app.js" (fun req -> Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req -> Dream.from_filesystem doc_root "Index.re.css" req);
  ]
