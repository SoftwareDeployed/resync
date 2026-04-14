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
  ( "middleware websocket behavior",
    [
      Alcotest.test_case "ping replies with pong" `Quick (fun () ->
          let adapter = Fake_adapter.create () in
          let runtime = make_runtime adapter in
          let sent = ref [] in
          let next_channel =
            Lwt_main.run
              (Middleware.handle_message_with_io runtime request None "ping"
                 ~send:(fun message ->
                   sent := message :: !sent;
                   Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun channel -> Lwt.return_some channel))
          in
          match next_channel with
          | None when !sent = [ Middleware.pong_message ] -> ()
          | _ -> Alcotest.fail "Expected ping to preserve empty channel and expose pong message");
      Alcotest.test_case "select subscribes and sends snapshot" `Quick (fun () ->
          let adapter = Fake_adapter.create () in
          let runtime = make_runtime adapter in
          let sent = ref [] in
          let next_channel =
            Lwt_main.run
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"select\",\"subscription\":\"room-1\"}"
                 ~send:(fun message ->
                   sent := message :: !sent;
                   Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun channel ->
                   Hashtbl.replace adapter.subscriptions channel (fun ?wrap:_ _ -> Lwt.return_unit);
                   sent := Middleware.wrap_snapshot "{}" :: !sent;
                   Lwt.return_some channel))
          in
          match next_channel with
          | Some "room-1" ->
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
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"actionId\":\"a-1\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun message ->
                   sent := message :: !sent;
                   Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun channel -> Lwt.return_some channel))
          in
          let payload = Middleware.ack_message ~action_id:"a-1" ~status:"ok" () in
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
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"actionId\":\"dup-1\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun _ -> Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun _ -> Lwt.return_some "c"))
          in
          let _ =
            Lwt_main.run
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"actionId\":\"dup-1\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun _ -> Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun _ -> Lwt.return_some "c"))
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
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"actionId\":\"fail-1\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun message ->
                   sent := message :: !sent;
                   Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun _ -> Lwt.return_some "c"))
          in
          let _ =
            Lwt_main.run
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"actionId\":\"fail-1\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun message ->
                   sent := message :: !sent;
                   Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun _ -> Lwt.return_some "c"))
          in
          let payload = Middleware.ack_message ~action_id:"fail-1" ~status:"error" ~error:"bad" () in
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
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"actionId\":\"noack-1\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun _ -> Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun _ -> Lwt.return_some "c"))
          in
          let _ =
            Lwt_main.run
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"actionId\":\"noack-1\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun _ -> Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun _ -> Lwt.return_some "c"))
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
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"actionId\":\"db-1\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun _ -> Lwt.return_unit)
                 ~close:(fun () -> Lwt.return_unit)
                 ~subscribe:(fun _ -> Lwt.return_some "c"))
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
              (Middleware.handle_message_with_io runtime request None
                 "{\"type\":\"mutation\",\"action\":{\"kind\":\"noop\"}}"
                 ~send:(fun message ->
                   sent := message :: !sent;
                   Lwt.return_unit)
                 ~close:(fun () ->
                   closed := true;
                   Lwt.return_unit)
                 ~subscribe:(fun channel -> Lwt.return_some channel))
          in
          let payload =
            Middleware.ack_message ~action_id:"" ~status:"error"
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
              (Middleware.handle_message_with_io runtime request (Some "room-1")
                 "{\"type\":\"media\",\"payload\":{\"kind\":\"frame\"}}"
                 ~send:(fun message ->
                   sent := message :: !sent;
                   Lwt.return_unit)
                 ~close:(fun () ->
                   closed := true;
                   Lwt.return_unit)
                 ~subscribe:(fun channel -> Lwt.return_some channel))
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
            [{ websocket; current_subscription = Some "room-1"; pending_sends = []; send_in_progress = false }];
          let _ = Lwt_main.run (Middleware.detach_websocket runtime websocket) in
          if List.mem "room-1" !(adapter.unsubscribed) then ()
          else Alcotest.fail "Expected detach to unsubscribe channel");
    ] )
