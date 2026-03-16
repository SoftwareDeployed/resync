open Lwt.Syntax

type t = {
  adapter : Adapter.packed;
  resolve_subscription : Dream.request -> string -> string option Lwt.t;
  load_snapshot : Dream.request -> string -> string Lwt.t;
  subscriptions : (string, Dream.websocket list) Hashtbl.t;
}

let create ~adapter ~resolve_subscription ~load_snapshot =
  {
    adapter;
    resolve_subscription;
    load_snapshot;
    subscriptions = Hashtbl.create 32;
  }

let broadcast t channel message =
  match Hashtbl.find_opt t.subscriptions channel with
  | Some websockets ->
      Lwt_list.iter_p (fun websocket -> Dream.send websocket message) websockets
  | None -> Lwt.return_unit

let remove_websocket_from_channel t channel websocket =
  let remaining =
    match Hashtbl.find_opt t.subscriptions channel with
    | Some websockets -> List.filter (( != ) websocket) websockets
    | None -> []
  in
  match remaining with
  | [] ->
      Hashtbl.remove t.subscriptions channel;
      Adapter.unsubscribe t.adapter ~channel
  | _ ->
      Hashtbl.replace t.subscriptions channel remaining;
      Lwt.return_unit

let detach_websocket t current_channel websocket =
  match current_channel with
  | Some channel -> remove_websocket_from_channel t channel websocket
  | None -> Lwt.return_unit

let subscribe_websocket t request websocket current_channel channel =
  let* snapshot = t.load_snapshot request channel in
  let existing =
    match Hashtbl.find_opt t.subscriptions channel with
    | Some websockets -> websockets
    | None -> []
  in
  let was_unsubscribed =
    match existing with
    | [] -> true
    | _ -> false
  in
  let already_subscribed = List.exists (( == ) websocket) existing in
  let* () =
    if was_unsubscribed then
      Adapter.subscribe t.adapter ~channel ~handler:(broadcast t channel)
    else
      Lwt.return_unit
  in
  let* () =
    match current_channel with
    | Some current_channel when String.equal current_channel channel ->
        Lwt.return_unit
    | Some current_channel ->
        remove_websocket_from_channel t current_channel websocket
    | None -> Lwt.return_unit
  in
  let () =
    if already_subscribed then
      ()
    else
      Hashtbl.replace t.subscriptions channel (websocket :: existing)
  in
  let* () = Dream.send websocket snapshot in
  Lwt.return_some channel

let handle_message t request websocket current_channel message =
  match String.split_on_char ' ' message with
  | ["ping"] ->
      let* () = Dream.send websocket "pong" in
      Lwt.return current_channel
  | ["select"; selection] -> (
      let* channel = t.resolve_subscription request selection in
      match channel with
      | Some channel ->
          subscribe_websocket t request websocket current_channel channel
      | None -> Lwt.return current_channel)
  | _ ->
      let* () = Dream.send websocket "Unknown command" in
      Lwt.return current_channel

let rec websocket_handler t request websocket current_channel =
  let* message = Dream.receive websocket in
  match message with
  | None -> detach_websocket t current_channel websocket
  | Some payload ->
      let* next_channel =
        handle_message t request websocket current_channel payload
      in
      websocket_handler t request websocket next_channel

let route path t =
  Dream.get path (fun request ->
      Dream.websocket (fun websocket ->
          websocket_handler t request websocket None))
