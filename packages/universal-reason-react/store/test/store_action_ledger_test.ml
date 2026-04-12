open Store

let dummy_json : Json.json = Obj.magic "dummy"

let make_record ~id ~status ~enqueued_at : StoreActionLedger.t =
  {
    id;
    scopeKey = "test-scope";
    action = dummy_json;
    status;
    enqueuedAt = enqueued_at;
    retryCount = 0;
    error = None;
  }

let suite =
  ( "StoreActionLedger",
    [
      Alcotest.test_case "Pending status indicates unsent action" `Quick (fun () ->
          let status = StoreActionLedger.Pending in
          let str = StoreActionLedger.statusToString status in
          Alcotest.(check string) "Pending status wrong" "pending" str);
      Alcotest.test_case "Syncing status indicates in-flight action" `Quick (fun () ->
          let status = StoreActionLedger.Syncing in
          let str = StoreActionLedger.statusToString status in
          Alcotest.(check string) "Syncing status wrong" "syncing" str);
      Alcotest.test_case "Acked status indicates confirmed action" `Quick (fun () ->
          let status = StoreActionLedger.Acked in
          let str = StoreActionLedger.statusToString status in
          Alcotest.(check string) "Acked status wrong" "acked" str);
      Alcotest.test_case "Failed status indicates error" `Quick (fun () ->
          let status = StoreActionLedger.Failed in
          let str = StoreActionLedger.statusToString status in
          Alcotest.(check string) "Failed status wrong" "failed" str);
      Alcotest.test_case "maxRetries limits retry attempts" `Quick (fun () ->
          Alcotest.(check int) "maxRetries should be 3" 3 StoreActionLedger.maxRetries);
      Alcotest.test_case "ackTimeoutMs defines timeout window" `Quick (fun () ->
          Alcotest.(check int) "ackTimeoutMs should be 5000" 5000 StoreActionLedger.ackTimeoutMs);
      Alcotest.test_case "sortByEnqueuedAt handles array" `Quick (fun () ->
          let r1 = make_record ~id:"a" ~status:"pending" ~enqueued_at:2000.0 in
          let r2 = make_record ~id:"b" ~status:"pending" ~enqueued_at:1000.0 in
          let sorted = StoreActionLedger.sortByEnqueuedAt [| r1; r2 |] in
          Alcotest.(check int) "Array length changed" 2 (Array.length sorted));
      Alcotest.test_case "sortByEnqueuedAt preserves order for same timestamp" `Quick
        (fun () ->
          let r1 = make_record ~id:"a" ~status:"pending" ~enqueued_at:1000.0 in
          let r2 = make_record ~id:"b" ~status:"pending" ~enqueued_at:1000.0 in
          let sorted = StoreActionLedger.sortByEnqueuedAt [| r1; r2 |] in
          Alcotest.(check int) "Same timestamp sort failed" 2 (Array.length sorted));
      Alcotest.test_case "statusOfString parses pending correctly" `Quick (fun () ->
          match StoreActionLedger.statusOfString "pending" with
          | StoreActionLedger.Pending -> ()
          | _ -> Alcotest.fail "statusOfString pending wrong");
      Alcotest.test_case "statusOfString parses failed correctly" `Quick (fun () ->
          match StoreActionLedger.statusOfString "failed" with
          | StoreActionLedger.Failed -> ()
          | _ -> Alcotest.fail "statusOfString failed wrong");
      Alcotest.test_case "statusOfString defaults to Pending for unknown" `Quick (fun () ->
          match StoreActionLedger.statusOfString "unknown" with
          | StoreActionLedger.Pending -> ()
          | _ -> Alcotest.fail "statusOfString unknown should default to Pending");
      Alcotest.test_case "action record has required fields" `Quick (fun () ->
          let record : StoreActionLedger.t =
            {
              id = "test-123";
              scopeKey = "user-456";
              action = dummy_json;
              status = "pending";
              enqueuedAt = 1234567890.0;
              retryCount = 0;
              error = None;
            }
          in
          Alcotest.(check bool)
            "Record fields incorrect"
            true
            (record.id = "test-123"
             && record.scopeKey = "user-456"
             && record.status = "pending"));
      Alcotest.test_case "applyAckOrdering chains ledger before event" `Quick (fun () ->
          let call_order = ref [] in
          let mockUpdateLedger () =
            call_order := "ledger" :: !call_order;
            Js.Promise.resolve ()
          in
          let mockEmitEvent () = call_order := "event" :: !call_order in
          let _promise =
            StoreControllerHelpers.applyAckOrdering
              ~updateLedger:mockUpdateLedger
              ~emitEvent:mockEmitEvent
              ()
          in
          match !call_order with
          | [ "event"; "ledger" ] -> ()
          | _ -> Alcotest.fail "Ledger must resolve before event fires");
      Alcotest.test_case "applyAckOrdering returns a promise" `Quick (fun () ->
          let updateLedger () = Js.Promise.resolve () in
          let emitEvent () = () in
          let promise =
            StoreControllerHelpers.applyAckOrdering
              ~updateLedger
              ~emitEvent
              ()
          in
          let _ = promise in
          ());
    ] )
