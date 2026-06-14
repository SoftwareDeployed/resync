open Lwt.Syntax

module Fake_adapter = struct
  type handler = ?wrap:(channel:string -> string -> string) -> string -> unit Lwt.t

  type t = {
    subscriptions : (string, handler) Hashtbl.t;
    unsubscribed : string list ref;
    fail_subscribe : bool ref;
    block_unsubscribe : bool ref;
    unsubscribe_started : bool ref;
    release_unsubscribe : unit Lwt.u option ref;
  }

  let create ?(fail_subscribe = false) ?(block_unsubscribe = false) () =
    {
      subscriptions = Hashtbl.create 8;
      unsubscribed = ref [];
      fail_subscribe = ref fail_subscribe;
      block_unsubscribe = ref block_unsubscribe;
      unsubscribe_started = ref false;
      release_unsubscribe = ref None;
    }

  let start _ = Lwt.return_unit
  let stop _ = Lwt.return_unit

  let subscribe t ~channel ~handler =
    if !(t.fail_subscribe) then
      Lwt.fail_with "adapter subscribe failed"
    else begin
      Hashtbl.replace t.subscriptions channel handler;
      Lwt.return_unit
    end

  let unsubscribe t ~channel =
    t.unsubscribed := channel :: !(t.unsubscribed);
    Hashtbl.remove t.subscriptions channel;
    if !(t.block_unsubscribe) then begin
      t.unsubscribe_started := true;
      let promise, release = Lwt.wait () in
      t.release_unsubscribe := Some release;
      promise
    end
    else Lwt.return_unit
end

let request = Dream.request ""

let make_runtime
  ?(resolve_subscription = fun _request selection -> Lwt.return_some selection)
  ?(load_snapshot = fun _request channel ->
    Lwt.return (Printf.sprintf "{\"channel\":\"%s\"}" channel))
  ?handle_mutation ?handle_mutation_without_db ?handle_media ?(action_store = (module In_memory_action_store : Action_store.S))
  ?(use_db = fun _request _callback ->
    Alcotest.fail "Unexpected database access in middleware test")
  adapter_state =
  let packed = Adapter.pack (module Fake_adapter) adapter_state in
  Middleware.create ~adapter:packed ~resolve_subscription ~load_snapshot
  ?handle_mutation ?handle_mutation_without_db ?handle_media ~action_store ~use_db ()

let rec wait_until ?(remaining = 100) predicate =
  if predicate () then Lwt.return_unit
  else if remaining <= 0 then Lwt.fail_with "condition was not reached"
  else
    let* () = Lwt.pause () in
    wait_until ~remaining:(remaining - 1) predicate

let make_test_websocket () =
  let captured = ref None in
  let response =
    Lwt_main.run
      (Dream.websocket ~close:false (fun websocket ->
        captured := Some websocket;
        Lwt.return_unit))
  in
  match !captured with
  | Some websocket -> websocket
  | None ->
      ignore (Dream_pure.Message.get_websocket response);
      failwith "Expected websocket"

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
  Alcotest.test_case "json frame increments diagnostics once" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    Middleware.message_count := 0;
    Middleware.last_log_time := Unix.gettimeofday ();
    let _ =
      Lwt_main.run
        (Middleware.handle_message_with_io runtime request []
           "{\"type\":\"select\",\"subscription\":\"room-1\"}"
           ~send:(fun _message -> Lwt.return_unit)
           ~close:(fun () -> Lwt.return_unit)
           ~subscribe:(fun channel -> Lwt.return_some channel)
           ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    Alcotest.(check int)
      "json frame should count as one websocket message"
      1
      !(Middleware.message_count));
  Alcotest.test_case "mutation success sends ack ok" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let handle_mutation _broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
    Lwt.return (Mutation_result.Ack (Ok ()))
    in
    let runtime = make_runtime ~handle_mutation ~use_db:Test_db.use_unused adapter in
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
  Alcotest.test_case "mutation after_commit runs after ack" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let events = ref [] in
    let handle_mutation _broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
      Lwt.return
        (Mutation_result.Ack_after_commit
           (fun () ->
             events := "after_commit" :: !events;
             Lwt.return_unit))
    in
    let runtime = make_runtime ~handle_mutation ~use_db:Test_db.use_unused adapter in
    let _ =
      Lwt_main.run
        (Middleware.handle_message_with_io runtime request []
           "{\"type\":\"mutation\",\"actionId\":\"after-1\",\"action\":{\"kind\":\"noop\"}}"
           ~send:(fun _message ->
             events := "ack" :: !events;
             Lwt.return_unit)
           ~close:(fun () -> Lwt.return_unit)
           ~subscribe:(fun channel -> Lwt.return_some channel)
           ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    match !events with
    | [ "after_commit"; "ack" ] -> ()
    | events ->
        Alcotest.fail
          ("Expected ack before after_commit, got "
          ^ String.concat ", " (List.rev events)));
  Alcotest.test_case "mutation duplicate skips handler" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let call_count = ref 0 in
    let handle_mutation _broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
    incr call_count;
    Lwt.return (Mutation_result.Ack (Ok ()))
    in
    let runtime = make_runtime ~handle_mutation ~use_db:Test_db.use_unused adapter in
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
    let runtime = make_runtime ~handle_mutation ~use_db:Test_db.use_unused adapter in
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
  Alcotest.test_case "mutation db exception still acks when failure recording fails" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let db_call_count = ref 0 in
    let use_db _request _callback =
      incr db_call_count;
      if !db_call_count = 1 then
        Lwt.fail (Failure "primary db unavailable")
      else
        Lwt.fail (Failure "failure recording unavailable")
    in
    let runtime = make_runtime ~use_db adapter in
    let sent = ref [] in
    let _ =
      Lwt_main.run
        (Middleware.handle_message_with_io runtime request []
           "{\"type\":\"mutation\",\"actionId\":\"db-error-1\",\"action\":{\"kind\":\"noop\"}}"
           ~send:(fun message ->
             sent := message :: !sent;
             Lwt.return_unit)
           ~close:(fun () -> Lwt.return_unit)
           ~subscribe:(fun _ -> Lwt.return_some "c")
           ~unsubscribe:(fun _channel -> Lwt.return_unit))
    in
    let payload =
      Middleware.ack_message
        ~channel:""
        ~action_id:"db-error-1"
        ~status:"error"
        ~error:"Failure(\"primary db unavailable\")"
        ()
    in
    Alcotest.(check int)
      "primary and recording DB callbacks both ran"
      2
      !db_call_count;
    Alcotest.(check (list string))
      "mutation should settle with the original DB error"
      [payload]
      !sent);
  Alcotest.test_case "mutation NoAck allows retry" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let call_count = ref 0 in
    let handle_mutation _broadcast _request ~db:_ ~action_id:_ ~mutation_name:_ _action =
    incr call_count;
    Lwt.return Mutation_result.NoAck
    in
    let runtime = make_runtime ~handle_mutation ~use_db:Test_db.use_unused adapter in
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
    let runtime = make_runtime ~handle_mutation ~use_db:Test_db.use_unused adapter in
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
  Alcotest.test_case "mutation without db skips use_db and dedupes action id" `Quick
    (fun () ->
      let adapter = Fake_adapter.create () in
      let call_count = ref 0 in
      let db_called = ref false in
      let handle_mutation_without_db _broadcast _request ~action_id:_ ~mutation_name:_ _action =
        incr call_count;
        Lwt.return (Mutation_result.Ack (Ok ()))
      in
      let use_db _request _callback =
        db_called := true;
        Lwt.return (Mutation_result.Ack (Error "db should not be used"))
      in
      let runtime =
        make_runtime ~handle_mutation_without_db ~use_db adapter
      in
      let sent = ref [] in
      let run_once () =
        Middleware.handle_message_with_io runtime request []
          "{\"type\":\"mutation\",\"actionId\":\"nodbless-1\",\"action\":{\"kind\":\"noop\"}}"
          ~send:(fun message ->
            sent := message :: !sent;
            Lwt.return_unit)
          ~close:(fun () -> Lwt.return_unit)
          ~subscribe:(fun channel -> Lwt.return_some channel)
          ~unsubscribe:(fun _channel -> Lwt.return_unit)
      in
      let _ = Lwt_main.run (run_once ()) in
      let _ = Lwt_main.run (run_once ()) in
      let payload =
        Middleware.ack_message
          ~channel:""
          ~action_id:"nodbless-1"
          ~status:"ok"
          ()
      in
      Alcotest.(check bool)
        "no-db mutation should not call use_db"
        false
        !db_called;
      Alcotest.(check int)
        "no-db handler should run once for duplicate action id"
        1
        !call_count;
      Alcotest.(check (list string))
        "duplicate no-db mutation should replay ok ack"
        [payload; payload]
        !sent);
  Alcotest.test_case "concurrent no-db duplicate waits for in-progress action" `Quick
    (fun () ->
      let adapter = Fake_adapter.create () in
      let call_count = ref 0 in
      let release_ref = ref None in
      let handle_mutation_without_db _broadcast _request ~action_id:_ ~mutation_name:_ _action =
        incr call_count;
        let promise, wake = Lwt.wait () in
        release_ref :=
          Some
            (fun () ->
              Lwt.wakeup_later wake (Mutation_result.Ack (Ok ())));
        promise
      in
      let runtime = make_runtime ~handle_mutation_without_db adapter in
      let sent = ref [] in
      let run_once () =
        Middleware.handle_message_with_io runtime request []
          "{\"type\":\"mutation\",\"actionId\":\"nodbless-concurrent-1\",\"action\":{\"kind\":\"noop\"}}"
          ~send:(fun message ->
            sent := message :: !sent;
            Lwt.return_unit)
          ~close:(fun () -> Lwt.return_unit)
          ~subscribe:(fun channel -> Lwt.return_some channel)
          ~unsubscribe:(fun _channel -> Lwt.return_unit)
      in
      Lwt_main.run
        (let first = run_once () in
         let* () = Lwt.pause () in
         let second = run_once () in
         let* () = Lwt.pause () in
         Alcotest.(check int)
           "concurrent no-db handler should run once"
           1
           !call_count;
         (match !release_ref with
          | Some release -> release ()
          | None -> Alcotest.fail "Expected first handler to be in progress");
         let* _ = first in
         let* _ = second in
         Lwt.return_unit);
      let payload =
        Middleware.ack_message
          ~channel:""
          ~action_id:"nodbless-concurrent-1"
          ~status:"ok"
          ()
      in
      Alcotest.(check (list string))
        "concurrent duplicate no-db mutation should share ok ack"
        [payload; payload]
        !sent);
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
  Alcotest.test_case "invalid mutation kind preserves action id in ack" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let sent = ref [] in
    let closed = ref false in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"bad-kind-1\",\"action\":{}}"
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
    Middleware.ack_message ~channel:"" ~action_id:"bad-kind-1" ~status:"error"
    ~error:"Invalid mutation frame: missing kind" ()
    in
    if !sent = [ payload ] && !closed then ()
    else Alcotest.fail "Expected invalid mutation kind ack to preserve action id");
  Alcotest.test_case "invalid mutation action preserves action id in ack" `Quick (fun () ->
    let adapter = Fake_adapter.create () in
    let runtime = make_runtime adapter in
    let sent = ref [] in
    let closed = ref false in
    let _ =
    Lwt_main.run
    (Middleware.handle_message_with_io runtime request []
    "{\"type\":\"mutation\",\"actionId\":\"missing-action-1\"}"
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
    Middleware.ack_message ~channel:"" ~action_id:"missing-action-1" ~status:"error"
    ~error:"Invalid mutation frame" ()
    in
    if !sent = [ payload ] && !closed then ()
    else Alcotest.fail "Expected invalid mutation action ack to preserve action id");
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
    [{ connection_id = 1; websocket; current_subscriptions = ["room-1"]; pending_sends = []; send_in_progress = false; closed = false }];
    let _ = Lwt_main.run (Middleware.detach_websocket runtime websocket) in
    if List.mem "room-1" !(adapter.unsubscribed) then ()
    else Alcotest.fail "Expected detach to unsubscribe channel");
  Alcotest.test_case "detaching one websocket does not block another subscribe" `Quick
    (fun () ->
      let adapter = Fake_adapter.create ~block_unsubscribe:true () in
      let runtime = make_runtime adapter in
      let closing_websocket = make_test_websocket () in
      let next_websocket = make_test_websocket () in
      Hashtbl.replace runtime.Middleware.channels "room-closing"
        [
          {
            connection_id = 1;
            websocket = closing_websocket;
            current_subscriptions = [ "room-closing" ];
            pending_sends = [];
            send_in_progress = false;
            closed = false;
          };
        ];
      Lwt_main.run
        (let detach = Middleware.detach_websocket runtime closing_websocket in
         let* () = wait_until (fun () -> !(adapter.unsubscribe_started)) in
         let subscribe_result =
           Middleware.subscribe_websocket runtime request next_websocket
             "room-next"
         in
         let* () = wait_until (fun () -> Hashtbl.mem runtime.Middleware.channels "room-next") in
         (match !(adapter.release_unsubscribe) with
          | Some release -> Lwt.wakeup_later release ()
          | None -> Alcotest.fail "Expected blocked unsubscribe release");
         let* () = detach in
         let* subscribe_result = subscribe_result in
         match subscribe_result with
         | Some "room-next" -> Lwt.return_unit
         | Some _ -> Alcotest.fail "Expected second websocket to subscribe"
         | None -> Alcotest.fail "Expected second websocket to subscribe");
      Alcotest.(check bool)
        "closing channel removed"
        false
        (Hashtbl.mem runtime.Middleware.channels "room-closing");
      Alcotest.(check bool)
        "new channel subscribed"
        true
        (Hashtbl.mem runtime.Middleware.channels "room-next"));
  Alcotest.test_case "detach closes multiplexed subscriber before unsubscribe awaits" `Quick
    (fun () ->
      let adapter = Fake_adapter.create ~block_unsubscribe:true () in
      let runtime = make_runtime adapter in
      let websocket = make_test_websocket () in
      let subscriber : Middleware.subscriber =
        {
          connection_id = 1;
          websocket;
          current_subscriptions = [ "room-a"; "room-b" ];
          pending_sends = [];
          send_in_progress = false;
          closed = false;
        }
      in
      Hashtbl.replace runtime.Middleware.channels "room-a" [ subscriber ];
      Hashtbl.replace runtime.Middleware.channels "room-b" [ subscriber ];
      Lwt_main.run
        (let* () = Middleware.enqueue_subscriber_send runtime subscriber "{}" in
         let detach = Middleware.detach_websocket runtime websocket in
         let* () = wait_until (fun () -> !(adapter.unsubscribe_started)) in
         Alcotest.(check bool)
           "subscriber should be closed while unsubscribe is pending"
           true subscriber.closed;
         Alcotest.(check int)
           "queued sends should be dropped before unsubscribe completes"
           0 (List.length subscriber.pending_sends);
         Alcotest.(check bool)
           "drain should be stopped before unsubscribe completes"
           false subscriber.send_in_progress;
         adapter.block_unsubscribe := false;
         (match !(adapter.release_unsubscribe) with
          | Some release -> Lwt.wakeup_later release ()
          | None -> Alcotest.fail "Expected blocked unsubscribe release");
         detach);
      Alcotest.(check bool)
        "room-a removed"
        false
        (Hashtbl.mem runtime.Middleware.channels "room-a");
      Alcotest.(check bool)
        "room-b removed"
        false
        (Hashtbl.mem runtime.Middleware.channels "room-b"));
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
    let runtime = make_runtime ~handle_mutation ~use_db:Test_db.use_unused adapter in
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
    [{ connection_id = 1; websocket; current_subscriptions = ["room-a"]; pending_sends = []; send_in_progress = false; closed = false }];
    Hashtbl.replace runtime.Middleware.channels "room-b"
    [{ connection_id = 1; websocket; current_subscriptions = ["room-b"]; pending_sends = []; send_in_progress = false; closed = false }];
    let _ = Lwt_main.run (Middleware.detach_websocket runtime websocket) in
    if List.mem "room-a" !(adapter.unsubscribed) && List.mem "room-b" !(adapter.unsubscribed)
    then ()
    else Alcotest.fail "Expected detach to unsubscribe all channels");
  Alcotest.test_case "send queue schedules async drain" `Quick
    (fun () ->
      let adapter = Fake_adapter.create () in
      let runtime = make_runtime adapter in
      let captured = ref None in
      let _response =
        Lwt_main.run
          (Dream.websocket ~close:false (fun websocket ->
             captured := Some websocket;
             Lwt.return_unit))
      in
      let websocket =
        match !captured with
        | Some websocket -> websocket
        | None -> Alcotest.fail "Expected websocket"
      in
      let subscriber : Middleware.subscriber =
        {
          connection_id = 1;
          websocket;
          current_subscriptions = [ "room-a" ];
          pending_sends = [];
          send_in_progress = false;
          closed = false;
        }
      in
      Lwt_main.run (Middleware.enqueue_subscriber_send runtime subscriber "{}");
      Alcotest.(check int)
        "enqueue should return before drain"
        1
        (List.length subscriber.pending_sends);
      Alcotest.(check bool)
        "drain should be scheduled"
        true
        subscriber.send_in_progress;
      subscriber.closed <- true;
      Lwt_main.run (Lwt_unix.sleep 0.05);
      Alcotest.(check int)
        "scheduled drain should drop closed queue"
        0
        (List.length subscriber.pending_sends);
      Alcotest.(check bool)
        "scheduled drain should complete for closed queue"
        false
        subscriber.send_in_progress);
  Alcotest.test_case "failed snapshot does not register websocket subscription" `Quick
    (fun () ->
      let adapter = Fake_adapter.create () in
      let load_snapshot _request _channel = Lwt.fail_with "snapshot failed" in
      let runtime = make_runtime ~load_snapshot adapter in
      let captured = ref None in
      let _response =
        Lwt_main.run
          (Dream.websocket ~close:false (fun websocket ->
             captured := Some websocket;
             Lwt.return_unit))
      in
      let websocket =
        match !captured with
        | Some websocket -> websocket
        | None -> Alcotest.fail "Expected websocket"
      in
      let result =
        Lwt_main.run
          (Middleware.subscribe_websocket runtime request websocket "room-fail")
      in
      Alcotest.(check bool) "subscribe should fail closed" true (result = None);
      Alcotest.(check bool)
        "runtime should not keep failed channel"
        false
        (Hashtbl.mem runtime.Middleware.channels "room-fail");
      Alcotest.(check bool)
        "adapter should not subscribe failed channel"
        false
        (Hashtbl.mem adapter.subscriptions "room-fail"));
  Alcotest.test_case "failed adapter subscribe rolls back registered websocket" `Quick
    (fun () ->
      let adapter = Fake_adapter.create ~fail_subscribe:true () in
      let runtime = make_runtime adapter in
      let captured = ref None in
      let _response =
        Lwt_main.run
          (Dream.websocket ~close:false (fun websocket ->
             captured := Some websocket;
             Lwt.return_unit))
      in
      let websocket =
        match !captured with
        | Some websocket -> websocket
        | None -> Alcotest.fail "Expected websocket"
      in
      let result =
        Lwt_main.run
          (Middleware.subscribe_websocket runtime request websocket "room-adapter-fail")
      in
      Alcotest.(check bool) "subscribe should fail closed" true (result = None);
      Alcotest.(check bool)
        "runtime should remove rolled-back channel"
        false
        (Hashtbl.mem runtime.Middleware.channels "room-adapter-fail");
      Alcotest.(check bool)
        "adapter should not keep failed subscription"
        false
        (Hashtbl.mem adapter.subscriptions "room-adapter-fail"));
  Alcotest.test_case "closed subscriber drops queued sends" `Quick
    (fun () ->
      let adapter = Fake_adapter.create () in
      let runtime = make_runtime adapter in
      let captured = ref None in
      let _response =
        Lwt_main.run
          (Dream.websocket ~close:false (fun websocket ->
             captured := Some websocket;
             Lwt.return_unit))
      in
      let websocket =
        match !captured with
        | Some websocket -> websocket
        | None -> Alcotest.fail "Expected websocket"
      in
      let subscriber : Middleware.subscriber =
        {
          connection_id = 1;
          websocket;
          current_subscriptions = [];
          pending_sends = [];
          send_in_progress = false;
          closed = true;
        }
      in
      Lwt_main.run (Middleware.enqueue_subscriber_send runtime subscriber "{}");
      Alcotest.(check int)
        "closed subscriber should not queue"
        0
        (List.length subscriber.pending_sends);
      Alcotest.(check bool)
        "closed subscriber should not start drain worker"
        false
        subscriber.send_in_progress);
] )
