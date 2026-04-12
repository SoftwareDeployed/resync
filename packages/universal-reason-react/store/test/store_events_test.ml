let suite =
  ( "StoreEvents",
    [
      Alcotest.test_case "listen registers callback in registry" `Quick (fun () ->
          let registry : string StoreEvents.registry = ref [||] in
          let called = ref false in
          let callback (_event : string StoreEvents.store_event) = called := true in
          let _id = StoreEvents.Events.listen ~registry callback in
          Alcotest.(check int)
            "Registry should have one listener"
            1
            (Array.length !registry));
      Alcotest.test_case "emit delivers event to registered listener" `Quick (fun () ->
          let registry : unit StoreEvents.registry = ref [||] in
          let received = ref None in
          let callback (event : unit StoreEvents.store_event) = received := Some event in
          let _id = StoreEvents.Events.listen ~registry callback in
          StoreEvents.Events.emit ~registry StoreEvents.Open;
          match !received with
          | Some StoreEvents.Open -> ()
          | Some _ -> Alcotest.fail "Wrong event received"
          | None -> Alcotest.fail "Event not delivered to listener");
      Alcotest.test_case "unlisten removes callback from registry" `Quick (fun () ->
          let registry : unit StoreEvents.registry = ref [||] in
          let callback (_event : unit StoreEvents.store_event) = () in
          let id = StoreEvents.Events.listen ~registry callback in
          let before_count = Array.length !registry in
          StoreEvents.Events.unlisten ~registry id;
          let after_count = Array.length !registry in
          Alcotest.(check bool)
            "Listener not removed"
            true
            (before_count = 1 && after_count = 0));
      Alcotest.test_case "emit delivers to multiple listeners" `Quick (fun () ->
          let registry : unit StoreEvents.registry = ref [||] in
          let count = ref 0 in
          let callback1 (_event : unit StoreEvents.store_event) = count := !count + 1 in
          let callback2 (_event : unit StoreEvents.store_event) = count := !count + 1 in
          let _id1 = StoreEvents.Events.listen ~registry callback1 in
          let _id2 = StoreEvents.Events.listen ~registry callback2 in
          StoreEvents.Events.emit ~registry StoreEvents.Close;
          Alcotest.(check int)
            (Printf.sprintf "Expected 2 calls, got %d" !count)
            2
            !count);
      Alcotest.test_case "ActionAcked event carries action id" `Quick (fun () ->
          let ack = StoreEvents.ActionAcked { actionId = "action-123"; action = Some () } in
          match ack with
          | StoreEvents.ActionAcked { actionId; _ } when actionId = "action-123" -> ()
          | _ -> Alcotest.fail "ActionAcked not constructed correctly");
      Alcotest.test_case "ActionFailed event carries error message" `Quick (fun () ->
          let failure =
            StoreEvents.ActionFailed { actionId = "action-456"; action = None; message = "timeout" }
          in
          match failure with
          | StoreEvents.ActionFailed { message; _ } when message = "timeout" -> ()
          | _ -> Alcotest.fail "ActionFailed not constructed correctly");
      Alcotest.test_case "isEmitting returns false when not emitting" `Quick (fun () ->
          let state = StoreControllerHelpers.makeEmissionState () in
          Alcotest.(check bool)
            "Should not be emitting initially"
            false
            (StoreControllerHelpers.isEmitting state));
      Alcotest.test_case "shouldQueueDispatch returns false when not emitting" `Quick
        (fun () ->
          let state = StoreControllerHelpers.makeEmissionState () in
          Alcotest.(check bool)
            "Should not queue when not emitting"
            false
            (StoreControllerHelpers.shouldQueueDispatch state));
      Alcotest.test_case "emitWithQueuedDispatch sets isEmitting during listener callback" `Quick
        (fun () ->
          let state = StoreControllerHelpers.makeEmissionState () in
          let observed_emitting = ref false in
          let listener (_ : string StoreEvents.store_event) =
            observed_emitting := StoreControllerHelpers.isEmitting state
          in
          let listeners : (string * (string StoreEvents.store_event -> unit)) array =
            [| ("id", listener) |]
          in
          StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
          Alcotest.(check bool)
            "isEmitting should be true during listener"
            true
            !observed_emitting);
      Alcotest.test_case "emitWithQueuedDispatch clears isEmitting after listeners complete" `Quick
        (fun () ->
          let state = StoreControllerHelpers.makeEmissionState () in
          let listener (_ : string StoreEvents.store_event) = () in
          let listeners : (string * (string StoreEvents.store_event -> unit)) array =
            [| ("id", listener) |]
          in
          StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
          Alcotest.(check bool)
            "isEmitting should be false after emit completes"
            true
            (StoreControllerHelpers.isEmitting state = false));
      Alcotest.test_case "dispatch queued during emit is executed after emit completes" `Quick
        (fun () ->
          let state = StoreControllerHelpers.makeEmissionState () in
          let dispatch_executed = ref false in
          let listener (_ : string StoreEvents.store_event) =
            StoreControllerHelpers.queueDispatch state (fun () -> dispatch_executed := true)
          in
          let listeners : (string * (string StoreEvents.store_event -> unit)) array =
            [| ("id", listener) |]
          in
          StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
          Alcotest.(check bool)
            "Queued dispatch should execute after emit"
            true
            !dispatch_executed);
      Alcotest.test_case "queued dispatches execute in FIFO order" `Quick (fun () ->
          let state = StoreControllerHelpers.makeEmissionState () in
          let execution_order = ref [] in
          let listener (_ : string StoreEvents.store_event) =
            StoreControllerHelpers.queueDispatch state (fun () ->
              execution_order := 1 :: !execution_order);
            StoreControllerHelpers.queueDispatch state (fun () ->
              execution_order := 2 :: !execution_order);
            StoreControllerHelpers.queueDispatch state (fun () ->
              execution_order := 3 :: !execution_order)
          in
          let listeners : (string * (string StoreEvents.store_event -> unit)) array =
            [| ("id", listener) |]
          in
          StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
          Alcotest.(check (list int))
            "Dispatches not in FIFO order"
            [ 3; 2; 1 ]
            !execution_order);
      Alcotest.test_case "nested dispatch does not execute synchronously during emit" `Quick
        (fun () ->
          let state = StoreControllerHelpers.makeEmissionState () in
          let during_emit = ref false in
          let after_emit = ref false in
          let listener (_ : string StoreEvents.store_event) =
            StoreControllerHelpers.queueDispatch state (fun () ->
              during_emit := StoreControllerHelpers.isEmitting state;
              after_emit := true)
          in
          let listeners : (string * (string StoreEvents.store_event -> unit)) array =
            [| ("id", listener) |]
          in
          StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
          Alcotest.(check bool)
            "Dispatch should execute after emit, not during"
            true
            (!during_emit = false && !after_emit = true));
    ] )
