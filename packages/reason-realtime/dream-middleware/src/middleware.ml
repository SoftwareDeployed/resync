open Lwt.Syntax

type t = {
  adapter : Adapter.packed;
  resolve_subscription : Dream.request -> string -> string option Lwt.t;
  load_snapshot : Dream.request -> string -> string Lwt.t;
  handle_mutation : (Dream.request -> action_id:string -> Yojson.Basic.t -> (unit, string) result Lwt.t) option;
  subscriptions : (string, Dream.websocket list) Hashtbl.t;
}

let create ~adapter ~resolve_subscription ~load_snapshot ?handle_mutation () =
  {
    adapter;
    resolve_subscription;
    load_snapshot;
    handle_mutation;
    subscriptions = Hashtbl.create 32;
  }

let json_string value = Yojson.Basic.to_string value

let wrap_patch message =
  let payload =
    try Yojson.Basic.from_string message with _ -> `String message
  in
  `Assoc
    [
      ("type", `String "patch");
      ("timestamp", `Float (Unix.gettimeofday () *. 1000.0));
      ("payload", payload);
    ]
  |> json_string

let wrap_snapshot message =
  let payload =
    try Yojson.Basic.from_string message with _ -> `String message
  in
  `Assoc [ ("type", `String "snapshot"); ("payload", payload) ] |> json_string

let ack_message ~action_id ~status ?error () =
  let fields = [ ("type", `String "ack"); ("actionId", `String action_id); ("status", `String status) ] in
  let fields =
    match error with
    | Some error -> ("error", `String error) :: fields
    | None -> fields
  in
  `Assoc fields |> json_string

let pong_message = `Assoc [ ("type", `String "pong") ] |> json_string

let broadcast t channel message =
  match Hashtbl.find_opt t.subscriptions channel with
  | Some websockets ->
      let wrapped = wrap_patch message in
      Lwt_list.iter_p (fun websocket -> Dream.send websocket wrapped) websockets
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
  let* () = Dream.send websocket (wrap_snapshot snapshot) in
  Lwt.return_some channel

let assoc_string key = function
  | `Assoc fields -> (
      match List.assoc_opt key fields with
      | Some (`String value) -> Some value
      | _ -> None)
  | _ -> None

let assoc_json key = function
  | `Assoc fields -> List.assoc_opt key fields
  | _ -> None

let handle_json_message t request websocket current_channel json =
  match assoc_string "type" json with
  | Some "ping" ->
      let* () = Dream.send websocket pong_message in
      Lwt.return current_channel
  | Some "select" -> (
      match assoc_string "subscription" json with
      | Some selection ->
          let* channel = t.resolve_subscription request selection in
          (match channel with
          | Some channel ->
              subscribe_websocket t request websocket current_channel channel
          | None -> Lwt.return current_channel)
      | None -> Lwt.return current_channel)
  | Some "mutation" -> (
      match (assoc_string "actionId" json, assoc_json "action" json, t.handle_mutation) with
      | Some action_id, Some action, Some handler ->
          let* result = handler request ~action_id action in
          let message =
            match result with
            | Ok () -> ack_message ~action_id ~status:"ok" ()
            | Error error -> ack_message ~action_id ~status:"error" ~error ()
          in
          let* () = Dream.send websocket message in
          Lwt.return current_channel
      | _ ->
          let* () = Dream.send websocket (ack_message ~action_id:"" ~status:"error" ~error:"Invalid mutation frame" ()) in
          Lwt.return current_channel)
  | _ -> Lwt.return current_channel

let handle_message t request websocket current_channel message =
  match message with
  | "ping" ->
      let* () = Dream.send websocket pong_message in
      Lwt.return current_channel
  | _ ->
      let json =
        try Some (Yojson.Basic.from_string message) with _ -> None
      in
      match json with
      | Some json -> handle_json_message t request websocket current_channel json
      | None ->
          let* () = Dream.send websocket (ack_message ~action_id:"" ~status:"error" ~error:"Unknown message" ()) in
          Lwt.return current_channel

let rec websocket_handler t request websocket current_channel =
  let* message = Dream.receive websocket in
  match message with
  | None -> detach_websocket t current_channel websocket
  | Some payload ->
      let* next_channel = handle_message t request websocket current_channel payload in
      websocket_handler t request websocket next_channel

let route path t =
  Dream.get path (fun request ->
      Dream.websocket (fun websocket -> websocket_handler t request websocket None))
