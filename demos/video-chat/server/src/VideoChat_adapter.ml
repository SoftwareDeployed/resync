open Lwt.Syntax

type peer_queue = {
  peer_id: string;
  queue: string Queue.t;
  mutable active: bool;
}

type t = {
  peer_rooms: (string, string) Hashtbl.t;  (* peer_id -> room_id *)
  send_queues: (string, peer_queue) Hashtbl.t;  (* peer_id -> queue *)
}

let create () =
  { peer_rooms = Hashtbl.create 16; send_queues = Hashtbl.create 16 }

let start _t = Lwt.return_unit
let stop _t = Lwt.return_unit

let subscribe t ~channel ~handler =
  let _ = handler in
  Lwt.return_unit

let unsubscribe t ~channel =
  Hashtbl.remove t.peer_rooms channel;
  (* Mark queue as inactive and clear it *)
  (match Hashtbl.find_opt t.send_queues channel with
  | Some q ->
    q.active <- false;
    Queue.clear q.queue;
    Hashtbl.remove t.send_queues channel
  | None -> ());
  Lwt.return_unit

let add_peer t ~room_id ~peer_id =
  Hashtbl.replace t.peer_rooms peer_id room_id;
  (* Create send queue for this peer *)
  let queue = { peer_id; queue = Queue.create (); active = true } in
  Hashtbl.replace t.send_queues peer_id queue

let remove_peer t ~room_id:_ ~peer_id =
  Hashtbl.remove t.peer_rooms peer_id;
  (* Mark queue as inactive and clear it *)
  (match Hashtbl.find_opt t.send_queues peer_id with
  | Some q ->
    q.active <- false;
    Queue.clear q.queue;
    Hashtbl.remove t.send_queues peer_id
  | None -> ())

let get_peers t ~room_id =
  Hashtbl.fold (fun peer_id rid acc ->
    if rid = room_id then peer_id :: acc else acc
  ) t.peer_rooms []

let get_room t ~peer_id =
  Hashtbl.find_opt t.peer_rooms peer_id

let get_all_peer_ids t =
  Hashtbl.fold (fun peer_id _ acc -> peer_id :: acc) t.peer_rooms []

let queue_media_frame t ~room_id ~except_id frame_data =
  let peers = get_peers t ~room_id in
  let max_frames = 10 in  (* Keep 10 most recent frames *)
  List.iter (fun peer_id ->
    if peer_id <> except_id then
      match Hashtbl.find_opt t.send_queues peer_id with
      | Some q when q.active ->
          (* Add new frame to queue *)
          Queue.push frame_data q.queue;
          (* If over limit, drop from front (oldest frames) *)
          while Queue.length q.queue > max_frames do
            ignore (Queue.pop q.queue)
          done
      | _ -> ()
  ) peers

let get_next_frame t ~peer_id =
  match Hashtbl.find_opt t.send_queues peer_id with
  | Some q when q.active && not (Queue.is_empty q.queue) ->
    Some (Queue.pop q.queue)
  | _ -> None

let has_pending_frames t ~peer_id =
  match Hashtbl.find_opt t.send_queues peer_id with
  | Some q when q.active -> not (Queue.is_empty q.queue)
  | _ -> false
