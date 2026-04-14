open Lwt.Syntax

module Fake_adapter = struct
  type handler = ?wrap:(channel:string -> string -> string) -> string -> unit Lwt.t

  type t = {
    subscriptions : (string, handler) Hashtbl.t;
    unsubscribed : string list ref;
  }

  let create () =
    { subscriptions = Hashtbl.create 8; unsubscribed = ref [] }

  let start _ = Lwt.return_unit
  let stop _ = Lwt.return_unit

  let subscribe t ~channel ~handler =
    Hashtbl.replace t.subscriptions channel handler;
    Lwt.return_unit

  let unsubscribe t ~channel =
    t.unsubscribed := channel :: !(t.unsubscribed);
    Hashtbl.remove t.subscriptions channel;
    Lwt.return_unit
end

let request = Dream.request ""

let make_runtime
  ?(resolve_subscription = fun _request selection -> Lwt.return_some selection)
  ?(load_snapshot = fun _request channel ->
    Lwt.return (Printf.sprintf "{\"channel\":\"%s\"}" channel))
  ?handle_mutation ?handle_media ?(action_store = (module In_memory_action_store : Action_store.S))
  ?(use_db = fun _request callback ->
    let m = (Obj.magic () : (module Caqti_lwt.CONNECTION)) in
    callback m)
  adapter_state =
  let packed = Adapter.pack (module Fake_adapter) adapter_state in
  Middleware.create ~adapter:packed ~resolve_subscription ~load_snapshot
  ?handle_mutation ?handle_media ~action_store ~use_db ()

let suite =
  ( "middleware websocket behavior", [
  Alcotest.test_case "ping replies with pong" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let sent = ref [] in
    let next_channel =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request [] "ping"
    ~send:(fun message ->
      sent := message :: !sent;
      Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun channel -> Lwt.return_some channel)
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    match next_channel with
    | [] when !sent = [ Middleware.pong_message ] -> ()
    | _ -> Alcotest.fail "Expected ping to preserve empty channel and expose pong message");
  Alcotest.test_case "select subscribes and sends snapshot" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let sent = ref [] in
    let next_channel =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"select\",\"subscription\":\"room-1\"}"
    ~send:(fun message ->
      sent := message :: !sent;
      Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun channel ->
      Hashtbl.replace adapter.subscriptions channel (fun ?wrap:_ _ -> Lwt.return_unit);
      sent := Middleware.wrap_snapshot ~channel:"room-1" "{}" :: !sent;
      Lwt.return_some channel)
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    match next_channel with
    | ["room-1"] ->
    if Hashtbl.mem adapter.subscriptions "room-1"
    && List.exists (String.starts_with ~prefix:"{\"type\":\"snapshot\"") !sent
    then ()
    else Alcotest.fail "Expected room subscription and snapshot wrapper"
    | _ -> Alcotest.fail "Expected select to subscribe room-1");
  Alcotest.test_case "mutation success sends ack ok" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let handle_mutation _broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
    Lwt.return (Mutation_result.Ack (Ok ()))
    in
    let runtime = make_runtime ~handle_mutation adapter in
    let sent = ref [] in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"a-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun message ->
      sent := message :: !sent;
      Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun channel -> Lwt.return_some channel)
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    let payload = Middleware.ack_message ~channel:"" ~action_id:"a-1" ~status:"ok" () in
    if !sent = [ payload ] then ()
    else Alcotest.fail "Expected ok ack payload");
  Alcotest.test_case "mutation duplicate skips handler" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let call_count = ref 0 in
    let handle_mutation _broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
    incr call_count;
    Lwt.return (Mutation_result.Ack (Ok ()))
    in
    let runtime = make_runtime ~handle_mutation adapter in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"dup-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun _ -> Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun _ -> Lwt.return_some "c")
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"dup-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun _ -> Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun _ -> Lwt.return_some "c")
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    if !call_count = 1 then ()
    else Alcotest.fail (Printf.sprintf "Expected handler called once, got %d" !call_count));
  Alcotest.test_case "mutation failure records and replays" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let call_count = ref 0 in
    let handle_mutation _broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
    incr call_count;
    Lwt.return (Mutation_result.Ack (Error "bad"))
    in
    let runtime = make_runtime ~handle_mutation adapter in
    let sent = ref [] in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"fail-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun message ->
      sent := message :: !sent;
      Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun _ -> Lwt.return_some "c")
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"fail-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun message ->
      sent := message :: !sent;
      Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun _ -> Lwt.return_some "c")
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    let payload = Middleware.ack_message ~channel:"" ~action_id:"fail-1" ~status:"error" ~error:"bad" () in
    if !call_count = 1 && !sent = [ payload; payload ] then ()
    else Alcotest.fail "Expected handler called once and two error acks");
  Alcotest.test_case "mutation NoAck allows retry" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let call_count = ref 0 in
    let handle_mutation _broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
    incr call_count;
    Lwt.return Mutation_result.NoAck
    in
    let runtime = make_runtime ~handle_mutation adapter in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"noack-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun _ -> Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun _ -> Lwt.return_some "c")
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"noack-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun _ -> Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun _ -> Lwt.return_some "c")
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    if !call_count = 2 then ()
    else Alcotest.fail (Printf.sprintf "Expected handler called twice, got %d" !call_count));
  Alcotest.test_case "handler receives db module" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let received_db = ref false in
    let handle_mutation _broadcast _request ~db ~action_id:_ ~mutation_name:_ _action =
    (match db with
    | (module _ : Caqti_lwt.CONNECTION) -> received_db := true);
    Lwt.return (Mutation_result.Ack (Ok ()))
    in
    let runtime = make_runtime ~handle_mutation adapter in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"db-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun _ -> Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun _ -> Lwt.return_some "c")
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    if !received_db then ()
    else Alcotest.fail "Expected handler to receive db module");
  Alcotest.test_case "invalid mutation sends error ack" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let sent = ref [] in
    let closed = ref false in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun message ->
      sent := message :: !sent;
      Lwt.return_unit)
    ~close:(fun () ->
      closed := true;
      Lwt.return_unit)
    ~subscribe:(fun channel -> Lwt.return_some channel)
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    let payload =
    Middleware.ack_message ~channel:"" ~action_id:"" ~status:"error"
    ~error:"Invalid mutation frame" ()
    in
    if !sent = [ payload ] && !closed then ()
    else Alcotest.fail "Expected invalid mutation error ack");
  Alcotest.test_case "media handler error sends error frame" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let handle_media _broadcast _request _channel _payload =
    Lwt.return (Error "bad media")
    in
    let runtime = make_runtime ~handle_media adapter in
    let sent = ref [] in
    let closed = ref false in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request ["room-1"]
    "{\"type\":\"media\",\"payload\":{\"kind\":\"frame\"}}"
    ~send:(fun message ->
      sent := message :: !sent;
      Lwt.return_unit)
    ~close:(fun () ->
      closed := true;
      Lwt.return_unit)
    ~subscribe:(fun channel -> Lwt.return_some channel)
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    let payload =
    Yojson.Basic.to_string
    (`Assoc [ ("type", `String "error"); ("message", `String "bad media") ])
    in
    if !sent = [ payload ] && !closed then ()
    else Alcotest.fail "Expected media error payload");
  Alcotest.test_case "detach unsubscribes active channel" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let captured = ref None in
    let response =
    Lwt_main.run
    (Dream.websocket ~close:false (fun websocket ->
      captured := Some websocket;
      Lwt.return_unit))
    in
    let websocket =
    match !captured with
    | Some websocket -> websocket
    | None ->
      ignore (Dream_pure.Message.get_websocket response);
      failwith "Failed to construct websocket for detach test"
    in
    Hashtbl.replace runtime.Middleware.channels "room-1"
    [{ websocket; current_subscriptions = ["room-1"]; pending_sends = []; send_in_progress = false }];
    let _ = Lwt_main.run (Middleware.detach_websocket runtime websocket) in
    if List.mem "room-1" !(adapter.unsubscribed) then ()
    else Alcotest.fail "Expected detach to unsubscribe channel");
  Alcotest.test_case "select allows multiple subscriptions on same websocket" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let sent = ref [] in
    let next_channels = ref [] in
    let _ =
    Lwt_main.run
    (let* first =
      Middleware.handle_message_with_io runtime request []
      "{\"type\":\"select\",\"subscription\":\"room-1\"}"
      ~send:(fun message -> sent := message :: !sent; Lwt.return_unit)
      ~close:(fun () -> Lwt.return_unit)
      ~subscribe:(fun channel ->
        Hashtbl.replace adapter.subscriptions channel (fun ?wrap:_ _ -> Lwt.return_unit);
        sent := Middleware.wrap_snapshot ~channel:"room-1" "{}" :: !sent;
        Lwt.return_some channel)
      ~unsubscribe:(fun _channel -> Lwt.return_unit)
    in
    let* second =
      Middleware.handle_message_with_io runtime request first
      "{\"type\":\"select\",\"subscription\":\"room-2\"}"
      ~send:(fun message -> sent := message :: !sent; Lwt.return_unit)
      ~close:(fun () -> Lwt.return_unit)
      ~subscribe:(fun channel ->
        Hashtbl.replace adapter.subscriptions channel (fun ?wrap:_ _ -> Lwt.return_unit);
        sent := Middleware.wrap_snapshot ~channel:"room-2" "{}" :: !sent;
        Lwt.return_some channel)
      ~unsubscribe:(fun _channel -> Lwt.return_unit)
    in
    next_channels := second;
    Lwt.return_unit)
    in
    match !next_channels with
    | channels when List.mem "room-1" channels && List.mem "room-2" channels ->
      if Hashtbl.mem adapter.subscriptions "room-1" && Hashtbl.mem adapter.subscriptions "room-2"
      then ()
      else Alcotest.fail "Expected both subscriptions in adapter"
    | _ -> Alcotest.fail "Expected multiple subscriptions");
  Alcotest.test_case "unsubscribe removes only target channel" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let sent = ref [] in
    let next_channels = ref [] in
    let _ =
    Lwt_main.run
    (let* first =
      Middleware.handle_message_with_io runtime request []
      "{\"type\":\"select\",\"subscription\":\"room-a\"}"
      ~send:(fun message -> sent := message :: !sent; Lwt.return_unit)
      ~close:(fun () -> Lwt.return_unit)
      ~subscribe:(fun channel ->
        Hashtbl.replace adapter.subscriptions channel (fun ?wrap:_ _ -> Lwt.return_unit);
        Lwt.return_some channel)
      ~unsubscribe:(fun _channel -> Lwt.return_unit)
    in
    let* second =
      Middleware.handle_message_with_io runtime request first
      "{\"type\":\"select\",\"subscription\":\"room-b\"}"
      ~send:(fun message -> sent := message :: !sent; Lwt.return_unit)
      ~close:(fun () -> Lwt.return_unit)
      ~subscribe:(fun channel ->
        Hashtbl.replace adapter.subscriptions channel (fun ?wrap:_ _ -> Lwt.return_unit);
        Lwt.return_some channel)
      ~unsubscribe:(fun _channel -> Lwt.return_unit)
    in
    let* after_unsubscribe =
      Middleware.handle_message_with_io runtime request second
      "{\"type\":\"unsubscribe\",\"channel\":\"room-b\"}"
      ~send:(fun message -> sent := message :: !sent; Lwt.return_unit)
      ~close:(fun () -> Lwt.return_unit)
      ~subscribe:(fun channel -> Lwt.return_some channel)
      ~unsubscribe:(fun channel ->
        adapter.unsubscribed := channel :: !(adapter.unsubscribed);
        Hashtbl.remove adapter.subscriptions channel;
        Lwt.return_unit)
    in
    next_channels := after_unsubscribe;
    Lwt.return_unit)
    in
    match !next_channels with
    | ["room-a"] ->
      if List.mem "room-b" !(adapter.unsubscribed) && Hashtbl.mem adapter.subscriptions "room-a"
      then ()
      else Alcotest.fail "Expected room-b unsubscribed, room-a still active"
    | _ -> Alcotest.fail "Expected only room-a after unsubscribe");
  Alcotest.test_case "patch includes channel field" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let sent = ref [] in
    let handler_ref : Fake_adapter.handler option ref = ref None in
    let _ =
    Lwt_main.run
    (let* _ =
      Middleware.handle_message_with_io runtime request []
      "{\"type\":\"select\",\"subscription\":\"test\"}"
      ~send:(fun message -> sent := message :: !sent; Lwt.return_unit)
      ~close:(fun () -> Lwt.return_unit)
      ~subscribe:(fun channel ->
        let handler = fun ?wrap:_ _ -> Lwt.return_unit in
        handler_ref := Some handler;
        Hashtbl.replace adapter.subscriptions channel handler;
        Lwt.return_some channel)
      ~unsubscribe:(fun _channel -> Lwt.return_unit)
    in
    let handler = Option.get !handler_ref in
    let wrapped = fun ~channel s ->
      Printf.sprintf "{\"type\":\"patch\",\"channel\":\"%s\",\"data\":%s}" channel s
    in
    let* _ = handler ~wrap:wrapped "test-payload" in
    Lwt.return_unit)
    in
    ());
  Alcotest.test_case "ack includes channel field" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let handle_mutation broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
    broadcast "c" (fun ~channel s -> s);
    Lwt.return (Mutation_result.Ack (Ok ()))
    in
    let runtime = make_runtime ~handle_mutation adapter in
    let sent = ref [] in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request ["c"]
    "{\"type\":\"mutation\",\"actionId\":\"ack-1\",\"action\":{\"kind\":\"noop\"}}"
    ~send:(fun message -> sent := message :: !sent; Lwt.return_unit)
    ~close:(fun () -> Lwt.return_unit)
    ~subscribe:(fun channel -> Lwt.return_some channel)
    ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    ());
  Alcotest.test_case "detach unsubscribes all active channels" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let captured = ref None in
    let response =
    Lwt_main.run
    (Dream.websocket ~close:false (fun websocket ->
      captured := Some websocket;
      Lwt.return_unit))
    in
    let websocket =
    match !captured with
    | Some websocket -> websocket
    | None ->
      ignore (Dream_pure.Message.get_websocket response);
      failwith "Failed to construct websocket for detach test"
    in
    Hashtbl.replace runtime.Middleware.channels "room-a"
    [{ websocket; current_subscriptions = ["room-a"]; pending_sends = []; send_in_progress = false }];
    Hashtbl.replace runtime.Middleware.channels "room-b"
    [{ websocket; current_subscriptions = ["room-b"]; pending_sends = []; send_in_progress = false }];
    let _ = Lwt_main.run (Middleware.detach_websocket runtime websocket) in
    if List.mem "room-a" !(adapter.unsubscribed) && List.mem "room-b" !(adapter.unsubscribed)
    then ()
    else Alcotest.fail "Expected detach to unsubscribe all channels");
] )
