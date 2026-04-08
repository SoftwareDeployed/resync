open Test_framework

(* Test the actual behavior of StoreEvents functions, not just constructors *)

let init () =
  describe "StoreEvents Behavior" (fun () ->
    test "listen adds listener to registry and returns unique id" (fun () ->
      let registry : unit StoreEvents.registry = ref [||] in
      let received = ref [] in
      let callback event = received := !received @ [event] in
      let id1 = StoreEvents.Events.listen ~registry callback in
      let id2 = StoreEvents.Events.listen ~registry callback in
      (* Each listen returns a unique ID *)
      if id1 <> id2 && Array.length !registry = 2
      then Passed
      else Failed "IDs not unique or registry size wrong");

    test "emit delivers correct event type to listener" (fun () ->
      let registry : string StoreEvents.registry = ref [||] in
      let last_event = ref None in
      let callback event = last_event := Some event in
      let _id = StoreEvents.Events.listen ~registry callback in
      
      StoreEvents.Events.emit ~registry StoreEvents.Open;
      (match !last_event with
       | Some StoreEvents.Open -> Passed
       | Some _ -> Failed "Wrong event type received"
       | None -> Failed "No event received"));

    test "emit delivers event with payload to listener" (fun () ->
      let registry : string StoreEvents.registry = ref [||] in
      let received_payload = ref None in
      let callback event =
        match event with
        | StoreEvents.ActionAcked { actionId; action } ->
            received_payload := Some (actionId, action)
        | _ -> ()
      in
      let _id = StoreEvents.Events.listen ~registry callback in
      
      StoreEvents.Events.emit ~registry (StoreEvents.ActionAcked { actionId = "action-123"; action = Some "test-action" });
      (match !received_payload with
       | Some ("action-123", Some "test-action") -> Passed
       | Some (id, _) -> Failed (Printf.sprintf "Wrong actionId: %s" id)
       | None -> Failed "No payload received"));

    test "unlisten prevents future event delivery" (fun () ->
      let registry : unit StoreEvents.registry = ref [||] in
      let count = ref 0 in
      let callback _event = count := !count + 1 in
      let id = StoreEvents.Events.listen ~registry callback in
      
      StoreEvents.Events.emit ~registry StoreEvents.Open;
      let count_before = !count in
      
      StoreEvents.Events.unlisten ~registry id;
      StoreEvents.Events.emit ~registry StoreEvents.Close;
      let count_after = !count in
      
      if count_before = 1 && count_after = 1
      then Passed
      else Failed "Listener still receiving events after unlisten");

    test "unlisten one listener does not affect others" (fun () ->
      let registry : unit StoreEvents.registry = ref [||] in
      let count1 = ref 0 in
      let count2 = ref 0 in
      let callback1 _event = count1 := !count1 + 1 in
      let callback2 _event = count2 := !count2 + 1 in
      let id1 = StoreEvents.Events.listen ~registry callback1 in
      let _id2 = StoreEvents.Events.listen ~registry callback2 in
      
      StoreEvents.Events.emit ~registry StoreEvents.Open;
      StoreEvents.Events.unlisten ~registry id1;
      StoreEvents.Events.emit ~registry StoreEvents.Close;
      
      if !count1 = 1 && !count2 = 2
      then Passed
      else Failed (Printf.sprintf "count1=%d, count2=%d" !count1 !count2));

    test "emit to empty registry does not raise" (fun () ->
      let registry : unit StoreEvents.registry = ref [||] in
      (* Should not raise *)
      StoreEvents.Events.emit ~registry StoreEvents.Open;
      Passed);

    test "ActionFailed contains error message" (fun () ->
      let failure = StoreEvents.ActionFailed { 
        actionId = "action-456"; 
        action = None; 
        message = "Network timeout" 
      } in
      match failure with
      | StoreEvents.ActionFailed { message; _ } when message = "Network timeout" -> Passed
      | _ -> Failed "ActionFailed message incorrect");

    test "ConnectionError wraps message" (fun () ->
      let error = StoreEvents.ConnectionError "WebSocket closed" in
      match error with
      | StoreEvents.ConnectionError msg when msg = "WebSocket closed" -> Passed
      | _ -> Failed "ConnectionError message incorrect");

    test "listener receives all event types" (fun () ->
      let registry : unit StoreEvents.registry = ref [||] in
      let events = ref [] in
      let callback event = events := !events @ [event] in
      let _id = StoreEvents.Events.listen ~registry callback in
      
      StoreEvents.Events.emit ~registry StoreEvents.Open;
      StoreEvents.Events.emit ~registry StoreEvents.Close;
      StoreEvents.Events.emit ~registry StoreEvents.Reconnect;
      
      match !events with
      | [StoreEvents.Open; StoreEvents.Close; StoreEvents.Reconnect] -> Passed
      | _ -> Failed "Events not received in correct order");
  )
