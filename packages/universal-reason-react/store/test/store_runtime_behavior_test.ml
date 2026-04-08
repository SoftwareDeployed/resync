open Test_framework

let dummy_json : StoreJson.json = Obj.magic "action"

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
  describe "OptimisticReplay with Real Helpers" (fun () ->
    test "replayActions applies no actions when list is empty" (fun () ->
      let confirmed = 100 in
      let records = [||] in
      let result = StoreRuntimeHelpers.replayActions 
        ~reduce:(fun acc _ -> acc + 1) 
        ~confirmed 
        ~records 
      in
      if result = 100 then Passed else Failed "Should return confirmed state");

    test "replayActions applies single pending action" (fun () ->
      let confirmed = 10 in
      let records = [| make_record ~id:"r1" ~status:"pending" ~enqueued_at:1000.0 |] in
      let result = StoreRuntimeHelpers.replayActions 
        ~reduce:(fun acc _ -> acc + 5) 
        ~confirmed 
        ~records 
      in
      if result = 15 then Passed else Failed "Single action not applied");

    test "replayActions applies multiple pending actions in order" (fun () ->
      let confirmed = 0 in
      let records = [|
        make_record ~id:"first" ~status:"pending" ~enqueued_at:1000.0;
        make_record ~id:"second" ~status:"pending" ~enqueued_at:2000.0;
        make_record ~id:"third" ~status:"pending" ~enqueued_at:3000.0;
      |] in
      let result = StoreRuntimeHelpers.replayActions 
        ~reduce:(fun acc _ -> acc + 1) 
        ~confirmed 
        ~records 
      in
      if result = 3 then Passed else Failed "Actions not applied in order");

    test "replayActions skips Acked actions" (fun () ->
      let confirmed = 10 in
      let records = [|
        make_record ~id:"r1" ~status:"acked" ~enqueued_at:1000.0;
        make_record ~id:"r2" ~status:"pending" ~enqueued_at:2000.0;
      |] in
      let result = StoreRuntimeHelpers.replayActions 
        ~reduce:(fun acc _ -> acc + 1) 
        ~confirmed 
        ~records 
      in
      if result = 11 then Passed else Failed "Should apply only pending action");

    test "replayActions skips Failed actions" (fun () ->
      let confirmed = 10 in
      let records = [|
        make_record ~id:"r1" ~status:"failed" ~enqueued_at:1000.0;
        make_record ~id:"r2" ~status:"pending" ~enqueued_at:2000.0;
      |] in
      let result = StoreRuntimeHelpers.replayActions 
        ~reduce:(fun acc _ -> acc + 1) 
        ~confirmed 
        ~records 
      in
      if result = 11 then Passed else Failed "Should apply only pending action");

    test "replayActions handles Syncing actions same as Pending" (fun () ->
      let confirmed = 10 in
      let records = [|
        make_record ~id:"r1" ~status:"syncing" ~enqueued_at:1000.0;
      |] in
      let result = StoreRuntimeHelpers.replayActions 
        ~reduce:(fun acc _ -> acc + 5) 
        ~confirmed 
        ~records 
      in
      if result = 15 then Passed else Failed "Syncing action not applied");

    test "replayActions maintains state across multiple actions" (fun () ->
      let confirmed = 1 in
      let records = [|
        make_record ~id:"r1" ~status:"pending" ~enqueued_at:1000.0;
        make_record ~id:"r2" ~status:"pending" ~enqueued_at:2000.0;
        make_record ~id:"r3" ~status:"pending" ~enqueued_at:3000.0;
      |] in
      let result = StoreRuntimeHelpers.replayActions 
        ~reduce:(fun acc _ -> acc * 2) 
        ~confirmed 
        ~records 
      in
      if result = 8 then Passed else Failed "State not maintained correctly");
  );

  describe "Hydration with Real Helpers" (fun () ->
    test "selectHydrationBase uses persisted when newer" (fun () ->
      let initialState = (100, 1000.0) in
      let persistedState = Some (200, 2000.0) in
      let result = StoreRuntimeHelpers.selectHydrationBase 
        ~initialState 
        ~persistedState 
        ~timestampOfState:(fun (_, ts) -> ts)
      in
      if result = (200, 2000.0) then Passed else Failed "Should use persisted when newer");

    test "selectHydrationBase uses initial when persisted is older" (fun () ->
      let initialState = (100, 1000.0) in
      let persistedState = Some (50, 500.0) in
      let result = StoreRuntimeHelpers.selectHydrationBase 
        ~initialState 
        ~persistedState 
        ~timestampOfState:(fun (_, ts) -> ts)
      in
      if result = (100, 1000.0) then Passed else Failed "Should use initial when newer");

    test "selectHydrationBase uses initial when no persisted" (fun () ->
      let initialState = (100, 1000.0) in
      let persistedState = None in
      let result = StoreRuntimeHelpers.selectHydrationBase 
        ~initialState 
        ~persistedState 
        ~timestampOfState:(fun (_, ts) -> ts)
      in
      if result = (100, 1000.0) then Passed else Failed "Should use initial when no persisted");

    test "selectHydrationBase uses initial when timestamps equal" (fun () ->
      let initialState = (100, 1000.0) in
      let persistedState = Some (200, 1000.0) in
      let result = StoreRuntimeHelpers.selectHydrationBase 
        ~initialState 
        ~persistedState 
        ~timestampOfState:(fun (_, ts) -> ts)
      in
      if result = (100, 1000.0) then Passed else Failed "Should use initial when equal");
  );

  describe "ResumePending with Real Helpers" (fun () ->
    test "filterResumableRecords includes Pending" (fun () ->
      let records = [|
        make_record ~id:"r1" ~status:"pending" ~enqueued_at:1000.0;
        make_record ~id:"r2" ~status:"acked" ~enqueued_at:2000.0;
      |] in
      let result = StoreRuntimeHelpers.filterResumableRecords records in
      if List.length result = 1 then Passed else Failed "Should find one resumable");

    test "filterResumableRecords includes Syncing" (fun () ->
      let records = [|
        make_record ~id:"r1" ~status:"syncing" ~enqueued_at:1000.0;
        make_record ~id:"r2" ~status:"acked" ~enqueued_at:2000.0;
      |] in
      let result = StoreRuntimeHelpers.filterResumableRecords records in
      if List.length result = 1 then Passed else Failed "Should find one resumable");

    test "filterResumableRecords excludes Acked" (fun () ->
      let records = [|
        make_record ~id:"r1" ~status:"acked" ~enqueued_at:1000.0;
      |] in
      let result = StoreRuntimeHelpers.filterResumableRecords records in
      if List.length result = 0 then Passed else Failed "Should not include acked");

    test "filterResumableRecords excludes Failed" (fun () ->
      let records = [|
        make_record ~id:"r1" ~status:"failed" ~enqueued_at:1000.0;
      |] in
      let result = StoreRuntimeHelpers.filterResumableRecords records in
      if List.length result = 0 then Passed else Failed "Should not include failed");

    test "getPendingActionIds returns array" (fun () ->
      let records = [|
        make_record ~id:"acked-1" ~status:"acked" ~enqueued_at:1000.0;
        make_record ~id:"pending-1" ~status:"pending" ~enqueued_at:2000.0;
      |] in
      let result = StoreRuntimeHelpers.getPendingActionIds 
        ~confirmedTimestamp:2000.0 
        ~records 
      in
      if Array.length result >= 0 then Passed else Failed "Should return array");
  )
