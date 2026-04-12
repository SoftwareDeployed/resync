(* Test the actual behavior of StoreEvents functions, not just constructors *)

let event_testable =
  Alcotest.testable
    (fun ppf _ -> Format.fprintf ppf "<event>")
    Stdlib.( = )

let suite =
  ( "StoreEvents Behavior",
    [
      Alcotest.test_case "listen adds listener to registry and returns unique id" `Quick
        (fun () ->
          let registry : unit StoreEvents.registry = ref [||] in
          let received = ref [] in
          let callback event = received := !received @ [ event ] in
          let id1 = StoreEvents.Events.listen ~registry callback in
          let id2 = StoreEvents.Events.listen ~registry callback in
          Alcotest.(check bool)
            "IDs not unique or registry size wrong"
            true
            (id1 <> id2 && Array.length !registry = 2));
      Alcotest.test_case "emit delivers correct event type to listener" `Quick (fun () ->
          let registry : string StoreEvents.registry = ref [||] in
          let last_event = ref None in
          let callback event = last_event := Some event in
          let _id = StoreEvents.Events.listen ~registry callback in
          StoreEvents.Events.emit ~registry StoreEvents.Open;
          match !last_event with
          | Some StoreEvents.Open -> ()
          | Some _ -> Alcotest.fail "Wrong event type received"
          | None -> Alcotest.fail "No event received");
      Alcotest.test_case "emit delivers event with payload to listener" `Quick (fun () ->
          let registry : string StoreEvents.registry = ref [||] in
          let received_payload = ref None in
          let callback event =
            match event with
            | StoreEvents.ActionAcked { actionId; action } ->
                received_payload := Some (actionId, action)
            | _ -> ()
          in
          let _id = StoreEvents.Events.listen ~registry callback in
          StoreEvents.Events.emit ~registry
            (StoreEvents.ActionAcked
               { actionId = "action-123"; action = Some "test-action" });
          match !received_payload with
          | Some ("action-123", Some "test-action") -> ()
          | Some (id, _) -> Alcotest.fail (Printf.sprintf "Wrong actionId: %s" id)
          | None -> Alcotest.fail "No payload received");
      Alcotest.test_case "unlisten prevents future event delivery" `Quick (fun () ->
          let registry : unit StoreEvents.registry = ref [||] in
          let count = ref 0 in
          let callback _event = count := !count + 1 in
          let id = StoreEvents.Events.listen ~registry callback in
          StoreEvents.Events.emit ~registry StoreEvents.Open;
          let count_before = !count in
          StoreEvents.Events.unlisten ~registry id;
          StoreEvents.Events.emit ~registry StoreEvents.Close;
          let count_after = !count in
          Alcotest.(check bool)
            "Listener still receiving events after unlisten"
            true
            (count_before = 1 && count_after = 1));
      Alcotest.test_case "unlisten one listener does not affect others" `Quick (fun () ->
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
          Alcotest.(check bool)
            (Printf.sprintf "count1=%d, count2=%d" !count1 !count2)
            true
            (!count1 = 1 && !count2 = 2));
      Alcotest.test_case "emit to empty registry does not raise" `Quick (fun () ->
          let registry : unit StoreEvents.registry = ref [||] in
          StoreEvents.Events.emit ~registry StoreEvents.Open);
      Alcotest.test_case "ActionFailed contains error message" `Quick (fun () ->
          let failure =
            StoreEvents.ActionFailed
              { actionId = "action-456"; action = None; message = "Network timeout" }
          in
          match failure with
          | StoreEvents.ActionFailed { message; _ } when message = "Network timeout" -> ()
          | _ -> Alcotest.fail "ActionFailed message incorrect");
      Alcotest.test_case "ConnectionError wraps message" `Quick (fun () ->
          let error = StoreEvents.ConnectionError "WebSocket closed" in
          match error with
          | StoreEvents.ConnectionError msg when msg = "WebSocket closed" -> ()
          | _ -> Alcotest.fail "ConnectionError message incorrect");
      Alcotest.test_case "listener receives all event types" `Quick (fun () ->
          let registry : unit StoreEvents.registry = ref [||] in
          let events = ref [] in
          let callback event = events := !events @ [ event ] in
          let _id = StoreEvents.Events.listen ~registry callback in
          StoreEvents.Events.emit ~registry StoreEvents.Open;
          StoreEvents.Events.emit ~registry StoreEvents.Close;
          StoreEvents.Events.emit ~registry StoreEvents.Reconnect;
          Alcotest.(check (list event_testable))
            "Events not received in correct order"
            [ StoreEvents.Open; StoreEvents.Close; StoreEvents.Reconnect ]
            !events);
    ] )
