open Test_framework

let init () =
  describe "StoreEvents Lifecycle" (fun () ->
    test "listen registers callback in registry" (fun () ->
      let registry : string StoreEvents.registry = ref [||] in
      let called = ref false in
      let callback (_event : string StoreEvents.store_event) = called := true in
      let _id = StoreEvents.Events.listen ~registry callback in
      if Array.length !registry = 1 then Passed else Failed "Registry should have one listener");

    test "emit delivers event to registered listener" (fun () ->
      let registry : unit StoreEvents.registry = ref [||] in
      let received = ref None in
      let callback (event : unit StoreEvents.store_event) = received := Some event in
      let _id = StoreEvents.Events.listen ~registry callback in
      StoreEvents.Events.emit ~registry StoreEvents.Open;
      match !received with
      | Some StoreEvents.Open -> Passed
      | Some _ -> Failed "Wrong event received"
      | None -> Failed "Event not delivered to listener");

    test "unlisten removes callback from registry" (fun () ->
      let registry : unit StoreEvents.registry = ref [||] in
      let callback (_event : unit StoreEvents.store_event) = () in
      let id = StoreEvents.Events.listen ~registry callback in
      let before_count = Array.length !registry in
      StoreEvents.Events.unlisten ~registry id;
      let after_count = Array.length !registry in
      if before_count = 1 && after_count = 0 then Passed else Failed "Listener not removed");

    test "emit delivers to multiple listeners" (fun () ->
      let registry : unit StoreEvents.registry = ref [||] in
      let count = ref 0 in
      let callback1 (_event : unit StoreEvents.store_event) = count := !count + 1 in
      let callback2 (_event : unit StoreEvents.store_event) = count := !count + 1 in
      let _id1 = StoreEvents.Events.listen ~registry callback1 in
      let _id2 = StoreEvents.Events.listen ~registry callback2 in
      StoreEvents.Events.emit ~registry StoreEvents.Close;
      if !count = 2 then Passed else Failed (Printf.sprintf "Expected 2 calls, got %d" !count));

    test "ActionAcked event carries action id" (fun () ->
      let ack = StoreEvents.ActionAcked { actionId = "action-123"; action = Some () } in
      match ack with
      | StoreEvents.ActionAcked { actionId; _ } when actionId = "action-123" -> Passed
      | _ -> Failed "ActionAcked not constructed correctly");

    test "ActionFailed event carries error message" (fun () ->
      let failure = StoreEvents.ActionFailed { actionId = "action-456"; action = None; message = "timeout" } in
      match failure with
      | StoreEvents.ActionFailed { message; _ } when message = "timeout" -> Passed
      | _ -> Failed "ActionFailed not constructed correctly");
  );

  describe "StoreController Queued Dispatch Ordering" (fun () ->
    test "isEmitting returns false when not emitting" (fun () ->
      let state = StoreControllerHelpers.makeEmissionState () in
      if StoreControllerHelpers.isEmitting state = false 
      then Passed 
      else Failed "Should not be emitting initially");

    test "shouldQueueDispatch returns false when not emitting" (fun () ->
      let state = StoreControllerHelpers.makeEmissionState () in
      if StoreControllerHelpers.shouldQueueDispatch state = false
      then Passed
      else Failed "Should not queue when not emitting");

    test "emitWithQueuedDispatch sets isEmitting during listener callback" (fun () ->
      let state = StoreControllerHelpers.makeEmissionState () in
      let observed_emitting = ref false in
      let listener (_ : string StoreEvents.store_event) =
        observed_emitting := StoreControllerHelpers.isEmitting state
      in
      let listeners : (string * (string StoreEvents.store_event -> unit)) array = [| ("id", listener) |] in
      StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
      if !observed_emitting = true 
      then Passed 
      else Failed "isEmitting should be true during listener");

    test "emitWithQueuedDispatch clears isEmitting after listeners complete" (fun () ->
      let state = StoreControllerHelpers.makeEmissionState () in
      let listener (_ : string StoreEvents.store_event) = () in
      let listeners : (string * (string StoreEvents.store_event -> unit)) array = [| ("id", listener) |] in
      StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
      if StoreControllerHelpers.isEmitting state = false
      then Passed
      else Failed "isEmitting should be false after emit completes");

    test "dispatch queued during emit is executed after emit completes" (fun () ->
      let state = StoreControllerHelpers.makeEmissionState () in
      let dispatch_executed = ref false in
      let listener (_ : string StoreEvents.store_event) =
        StoreControllerHelpers.queueDispatch state (fun () -> dispatch_executed := true)
      in
      let listeners : (string * (string StoreEvents.store_event -> unit)) array = [| ("id", listener) |] in
      StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
      if !dispatch_executed = true
      then Passed
      else Failed "Queued dispatch should execute after emit");

    test "queued dispatches execute in FIFO order" (fun () ->
      let state = StoreControllerHelpers.makeEmissionState () in
      let execution_order = ref [] in
      let listener (_ : string StoreEvents.store_event) =
        StoreControllerHelpers.queueDispatch state (fun () -> execution_order := 1 :: !execution_order);
        StoreControllerHelpers.queueDispatch state (fun () -> execution_order := 2 :: !execution_order);
        StoreControllerHelpers.queueDispatch state (fun () -> execution_order := 3 :: !execution_order)
      in
      let listeners : (string * (string StoreEvents.store_event -> unit)) array = [| ("id", listener) |] in
      StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
      match !execution_order with
      | [3; 2; 1] -> Passed
      | _ -> Failed "Dispatches not in FIFO order");

    test "nested dispatch does not execute synchronously during emit" (fun () ->
      let state = StoreControllerHelpers.makeEmissionState () in
      let during_emit = ref false in
      let after_emit = ref false in
      let listener (_ : string StoreEvents.store_event) =
        StoreControllerHelpers.queueDispatch state (fun () -> 
          during_emit := StoreControllerHelpers.isEmitting state;
          after_emit := true
        )
      in
      let listeners : (string * (string StoreEvents.store_event -> unit)) array = [| ("id", listener) |] in
      StoreControllerHelpers.emitWithQueuedDispatch state listeners StoreEvents.Open;
      if !during_emit = false && !after_emit = true
      then Passed
      else Failed "Dispatch should execute after emit, not during");
  )
