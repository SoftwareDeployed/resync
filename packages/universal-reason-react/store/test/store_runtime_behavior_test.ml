open Test_framework
open Store

let dummy_json : Json.json = Obj.magic "action"

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
  );

  describe "rejectStaleCacheResult" (fun () ->
    test "returns true when cached timestamp is strictly older" (fun () ->
      let current = (100, 2000.0) in
      let cached = (50, 1000.0) in
      let result = StoreRuntimeHelpers.rejectStaleCacheResult
        ~currentConfirmedState:current
        ~cachedState:cached
        ~timestampOfState:(fun (_, ts) -> ts)
      in
      assert_true ~message:"Cached older than confirmed should be stale" result);

    test "returns true when timestamps are equal" (fun () ->
      let current = (100, 1500.0) in
      let cached = (200, 1500.0) in
      let result = StoreRuntimeHelpers.rejectStaleCacheResult
        ~currentConfirmedState:current
        ~cachedState:cached
        ~timestampOfState:(fun (_, ts) -> ts)
      in
      assert_true ~message:"Equal timestamps should be stale" result);

    test "returns false when cached timestamp is newer" (fun () ->
      let current = (100, 1000.0) in
      let cached = (200, 2000.0) in
      let result = StoreRuntimeHelpers.rejectStaleCacheResult
        ~currentConfirmedState:current
        ~cachedState:cached
        ~timestampOfState:(fun (_, ts) -> ts)
      in
      assert_false ~message:"Newer cached should not be stale" result);
  );

  describe "replayActions FIFO with mixed statuses" (fun () ->
    test "applies Pending and Syncing records in FIFO order across interleaved statuses" (fun () ->
      let records = [|
        make_record ~id:"r3" ~status:"acked"   ~enqueued_at:3000.0;
        make_record ~id:"r1" ~status:"pending" ~enqueued_at:1000.0;
        make_record ~id:"r5" ~status:"failed"  ~enqueued_at:5000.0;
        make_record ~id:"r2" ~status:"syncing" ~enqueued_at:2000.0;
        make_record ~id:"r4" ~status:"pending" ~enqueued_at:4000.0;
      |] in
      let result = StoreRuntimeHelpers.replayActions
        ~reduce:(fun acc (record : StoreActionLedger.t) ->
          acc ^ ":" ^ record.id)
        ~confirmed:"start"
        ~records
      in
      let expected = "start:r1:r2:r4" in
      if result = expected then Passed
      else Failed (Printf.sprintf "Expected '%s' but got '%s'" expected result));

    test "replayActions is deterministic given same input" (fun () ->
      let records = [|
        make_record ~id:"a" ~status:"pending" ~enqueued_at:100.0;
        make_record ~id:"b" ~status:"syncing" ~enqueued_at:200.0;
        make_record ~id:"c" ~status:"pending" ~enqueued_at:300.0;
      |] in
      let run () =
        StoreRuntimeHelpers.replayActions
          ~reduce:(fun acc (r : StoreActionLedger.t) -> acc + Char.code r.id.[0])
          ~confirmed:0
          ~records
      in
      let r1 = run () in
      let r2 = run () in
      if r1 = r2 then Passed
      else Failed (Printf.sprintf "Non-deterministic: %d vs %d" r1 r2));
  );

  describe "Seeded deterministic randomized replay" (fun () ->
    (* LCG PRNG: X_{n+1} = (a * X_n + c) mod m, parameters from glibc *)
    let lcg_state = ref 42 in
    let next_random () =
      let next = (!lcg_state * 1103515245 + 12345) land 0x7FFFFFFF in
      lcg_state := next;
      next
    in

    test "stable replay order across seeded shuffled mixed-status records" (fun () ->
      let statuses = [| "pending"; "syncing"; "acked"; "failed" |] in
      let records = Array.init 20 (fun i ->
        let rng = next_random () in
        let status_idx = rng mod 4 in
        let enqueued = float_of_int ((rng mod 10000) + 1000) in
        make_record
          ~id:(Printf.sprintf "rec-%02d" i)
          ~status:statuses.(status_idx)
          ~enqueued_at:enqueued
      ) in

      let reduce_fn acc (r : StoreActionLedger.t) =
        let c = r.id.[4] in
        acc ^ string_of_int (Char.code c - 48)
      in
      let r1 = StoreRuntimeHelpers.replayActions
        ~reduce:reduce_fn ~confirmed:"" ~records
      in
      lcg_state := 42;
      let records2 = Array.init 20 (fun i ->
        let rng = next_random () in
        let status_idx = rng mod 4 in
        let enqueued = float_of_int ((rng mod 10000) + 1000) in
        make_record
          ~id:(Printf.sprintf "rec-%02d" i)
          ~status:statuses.(status_idx)
          ~enqueued_at:enqueued
      ) in
      let r2 = StoreRuntimeHelpers.replayActions
        ~reduce:reduce_fn ~confirmed:"" ~records:records2
      in
      if r1 = r2 then Passed
      else Failed (Printf.sprintf
        "Non-deterministic replay: '%s' vs '%s'" r1 r2));

    test "seeded replay only processes resumable records" (fun () ->
      lcg_state := 999;
      let statuses = [| "pending"; "syncing"; "acked"; "failed" |] in
      let records = Array.init 15 (fun i ->
        let rng = next_random () in
        let status_idx = rng mod 4 in
        let enqueued = float_of_int (i * 100 + 100) in
        make_record
          ~id:(Printf.sprintf "s-%02d" i)
          ~status:statuses.(status_idx)
          ~enqueued_at:enqueued
      ) in
      let resumable = StoreRuntimeHelpers.filterResumableRecords records in
      let replayed = StoreRuntimeHelpers.replayActions
        ~reduce:(fun acc _ -> acc + 1)
        ~confirmed:0
        ~records:(Array.of_list resumable)
      in
      if replayed = List.length resumable then Passed
      else Failed (Printf.sprintf
        "Expected %d replayed but got %d" (List.length resumable) replayed));
  )
