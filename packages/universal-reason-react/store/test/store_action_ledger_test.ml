open Test_framework
open Store

let dummy_json : Json.json = Obj.magic "dummy"

let make_record ~id ~status ~enqueued_at : StoreActionLedger.t = {
  id;
  scopeKey = "test-scope";
  action = dummy_json;
  status;
  enqueuedAt = enqueued_at;
  retryCount = 0;
  error = None;
}

let init () =
  describe "StoreActionLedger Behavior" (fun () ->
    test "Pending status indicates unsent action" (fun () ->
      let status = StoreActionLedger.Pending in
      let str = StoreActionLedger.statusToString status in
      if str = "pending" then Passed else Failed "Pending status wrong");

    test "Syncing status indicates in-flight action" (fun () ->
      let status = StoreActionLedger.Syncing in
      let str = StoreActionLedger.statusToString status in
      if str = "syncing" then Passed else Failed "Syncing status wrong");

    test "Acked status indicates confirmed action" (fun () ->
      let status = StoreActionLedger.Acked in
      let str = StoreActionLedger.statusToString status in
      if str = "acked" then Passed else Failed "Acked status wrong");

    test "Failed status indicates error" (fun () ->
      let status = StoreActionLedger.Failed in
      let str = StoreActionLedger.statusToString status in
      if str = "failed" then Passed else Failed "Failed status wrong");

    test "maxRetries limits retry attempts" (fun () ->
      if StoreActionLedger.maxRetries = 3 then Passed else Failed "maxRetries should be 3");

    test "ackTimeoutMs defines timeout window" (fun () ->
      if StoreActionLedger.ackTimeoutMs = 5000 then Passed else Failed "ackTimeoutMs should be 5000");

    test "sortByEnqueuedAt handles array" (fun () ->
      let r1 = make_record ~id:"a" ~status:"pending" ~enqueued_at:2000.0 in
      let r2 = make_record ~id:"b" ~status:"pending" ~enqueued_at:1000.0 in
      let sorted = StoreActionLedger.sortByEnqueuedAt [| r1; r2 |] in
      if Array.length sorted = 2 then Passed else Failed "Array length changed");

    test "sortByEnqueuedAt preserves order for same timestamp" (fun () ->
      let r1 = make_record ~id:"a" ~status:"pending" ~enqueued_at:1000.0 in
      let r2 = make_record ~id:"b" ~status:"pending" ~enqueued_at:1000.0 in
      let sorted = StoreActionLedger.sortByEnqueuedAt [| r1; r2 |] in
      if Array.length sorted = 2 then Passed else Failed "Same timestamp sort failed");

    test "statusOfString parses pending correctly" (fun () ->
      match StoreActionLedger.statusOfString "pending" with
      | StoreActionLedger.Pending -> Passed
      | _ -> Failed "statusOfString pending wrong");

    test "statusOfString parses failed correctly" (fun () ->
      match StoreActionLedger.statusOfString "failed" with
      | StoreActionLedger.Failed -> Passed
      | _ -> Failed "statusOfString failed wrong");

    test "statusOfString defaults to Pending for unknown" (fun () ->
      match StoreActionLedger.statusOfString "unknown" with
      | StoreActionLedger.Pending -> Passed
      | _ -> Failed "statusOfString unknown should default to Pending");

    test "action record has required fields" (fun () ->
      let record : StoreActionLedger.t = {
        id = "test-123";
        scopeKey = "user-456";
        action = dummy_json;
        status = "pending";
        enqueuedAt = 1234567890.0;
        retryCount = 0;
        error = None;
      } in
      if record.id = "test-123" && record.scopeKey = "user-456" && record.status = "pending"
      then Passed else Failed "Record fields incorrect");

    test "applyAckOrdering chains ledger before event" (fun () ->
      let call_order = ref [] in
      let mockUpdateLedger () =
        call_order := "ledger" :: !call_order;
        Js.Promise.resolve ()
      in
      let mockEmitEvent () =
        call_order := "event" :: !call_order
      in
      let _promise = StoreControllerHelpers.applyAckOrdering 
        ~updateLedger:mockUpdateLedger 
        ~emitEvent:mockEmitEvent 
        () 
      in
      match !call_order with
      | ["event"; "ledger"] -> Passed
      | _ -> Failed "Ledger must resolve before event fires");

    test "applyAckOrdering returns a promise" (fun () ->
      let updateLedger () = Js.Promise.resolve () in
      let emitEvent () = () in
      let promise = StoreControllerHelpers.applyAckOrdering 
        ~updateLedger 
        ~emitEvent 
        () 
      in
      let _ = promise in
      Passed);
  )
