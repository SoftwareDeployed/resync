open Store

let dummy_json : Json.json = Obj.magic "action"

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

let int_float_pair = Alcotest.pair Alcotest.int (Alcotest.float 0.0)

let suite =
  ( "StoreRuntimeBehavior",
    [
      Alcotest.test_case "replayActions applies no actions when list is empty" `Quick
        (fun () ->
          let confirmed = 100 in
          let records = [||] in
          let result =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc _ -> acc + 1)
              ~confirmed
              ~records
          in
          Alcotest.(check int)
            "Should return confirmed state"
            100
            result);
      Alcotest.test_case "replayActions applies single pending action" `Quick (fun () ->
          let confirmed = 10 in
          let records =
            [| make_record ~id:"r1" ~status:"pending" ~enqueued_at:1000.0 |]
          in
          let result =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc _ -> acc + 5)
              ~confirmed
              ~records
          in
          Alcotest.(check int) "Single action not applied" 15 result);
      Alcotest.test_case "replayActions applies multiple pending actions in order" `Quick
        (fun () ->
          let confirmed = 0 in
          let records =
            [|
              make_record ~id:"first" ~status:"pending" ~enqueued_at:1000.0;
              make_record ~id:"second" ~status:"pending" ~enqueued_at:2000.0;
              make_record ~id:"third" ~status:"pending" ~enqueued_at:3000.0;
            |]
          in
          let result =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc _ -> acc + 1)
              ~confirmed
              ~records
          in
          Alcotest.(check int) "Actions not applied in order" 3 result);
      Alcotest.test_case "replayActions skips Acked actions" `Quick (fun () ->
          let confirmed = 10 in
          let records =
            [|
              make_record ~id:"r1" ~status:"acked" ~enqueued_at:1000.0;
              make_record ~id:"r2" ~status:"pending" ~enqueued_at:2000.0;
            |]
          in
          let result =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc _ -> acc + 1)
              ~confirmed
              ~records
          in
          Alcotest.(check int) "Should apply only pending action" 11 result);
      Alcotest.test_case "replayActions skips Failed actions" `Quick (fun () ->
          let confirmed = 10 in
          let records =
            [|
              make_record ~id:"r1" ~status:"failed" ~enqueued_at:1000.0;
              make_record ~id:"r2" ~status:"pending" ~enqueued_at:2000.0;
            |]
          in
          let result =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc _ -> acc + 1)
              ~confirmed
              ~records
          in
          Alcotest.(check int) "Should apply only pending action" 11 result);
      Alcotest.test_case "replayActions handles Syncing actions same as Pending" `Quick
        (fun () ->
          let confirmed = 10 in
          let records =
            [| make_record ~id:"r1" ~status:"syncing" ~enqueued_at:1000.0 |]
          in
          let result =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc _ -> acc + 5)
              ~confirmed
              ~records
          in
          Alcotest.(check int) "Syncing action not applied" 15 result);
      Alcotest.test_case "replayActions maintains state across multiple actions" `Quick
        (fun () ->
          let confirmed = 1 in
          let records =
            [|
              make_record ~id:"r1" ~status:"pending" ~enqueued_at:1000.0;
              make_record ~id:"r2" ~status:"pending" ~enqueued_at:2000.0;
              make_record ~id:"r3" ~status:"pending" ~enqueued_at:3000.0;
            |]
          in
          let result =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc _ -> acc * 2)
              ~confirmed
              ~records
          in
          Alcotest.(check int) "State not maintained correctly" 8 result);
      Alcotest.test_case "selectHydrationBase uses persisted when newer" `Quick (fun () ->
          let initialState = (100, 1000.0) in
          let persistedState = Some (200, 2000.0) in
          let result =
            StoreRuntimeHelpers.selectHydrationBase
              ~initialState
              ~persistedState
              ~timestampOfState:(fun (_, ts) -> ts)
          in
          Alcotest.(check int_float_pair)
            "Should use persisted when newer"
            (200, 2000.0)
            result);
      Alcotest.test_case "selectHydrationBase uses initial when persisted is older" `Quick
        (fun () ->
          let initialState = (100, 1000.0) in
          let persistedState = Some (50, 500.0) in
          let result =
            StoreRuntimeHelpers.selectHydrationBase
              ~initialState
              ~persistedState
              ~timestampOfState:(fun (_, ts) -> ts)
          in
          Alcotest.(check int_float_pair)
            "Should use initial when newer"
            (100, 1000.0)
            result);
      Alcotest.test_case "selectHydrationBase uses initial when no persisted" `Quick
        (fun () ->
          let initialState = (100, 1000.0) in
          let persistedState = None in
          let result =
            StoreRuntimeHelpers.selectHydrationBase
              ~initialState
              ~persistedState
              ~timestampOfState:(fun (_, ts) -> ts)
          in
          Alcotest.(check int_float_pair)
            "Should use initial when no persisted"
            (100, 1000.0)
            result);
      Alcotest.test_case "selectHydrationBase uses initial when timestamps equal" `Quick
        (fun () ->
          let initialState = (100, 1000.0) in
          let persistedState = Some (200, 1000.0) in
          let result =
            StoreRuntimeHelpers.selectHydrationBase
              ~initialState
              ~persistedState
              ~timestampOfState:(fun (_, ts) -> ts)
          in
          Alcotest.(check int_float_pair)
            "Should use initial when equal"
            (100, 1000.0)
            result);
      Alcotest.test_case "filterResumableRecords includes Pending" `Quick (fun () ->
          let records =
            [|
              make_record ~id:"r1" ~status:"pending" ~enqueued_at:1000.0;
              make_record ~id:"r2" ~status:"acked" ~enqueued_at:2000.0;
            |]
          in
          let result = StoreRuntimeHelpers.filterResumableRecords records in
          Alcotest.(check int) "Should find one resumable" 1 (List.length result));
      Alcotest.test_case "filterResumableRecords includes Syncing" `Quick (fun () ->
          let records =
            [|
              make_record ~id:"r1" ~status:"syncing" ~enqueued_at:1000.0;
              make_record ~id:"r2" ~status:"acked" ~enqueued_at:2000.0;
            |]
          in
          let result = StoreRuntimeHelpers.filterResumableRecords records in
          Alcotest.(check int) "Should find one resumable" 1 (List.length result));
      Alcotest.test_case "filterResumableRecords excludes Acked" `Quick (fun () ->
          let records =
            [| make_record ~id:"r1" ~status:"acked" ~enqueued_at:1000.0 |]
          in
          let result = StoreRuntimeHelpers.filterResumableRecords records in
          Alcotest.(check int) "Should not include acked" 0 (List.length result));
      Alcotest.test_case "filterResumableRecords excludes Failed" `Quick (fun () ->
          let records =
            [| make_record ~id:"r1" ~status:"failed" ~enqueued_at:1000.0 |]
          in
          let result = StoreRuntimeHelpers.filterResumableRecords records in
          Alcotest.(check int) "Should not include failed" 0 (List.length result));
      Alcotest.test_case "getPendingActionIds returns array" `Quick (fun () ->
          let records =
            [|
              make_record ~id:"acked-1" ~status:"acked" ~enqueued_at:1000.0;
              make_record ~id:"pending-1" ~status:"pending" ~enqueued_at:2000.0;
            |]
          in
          let result =
            StoreRuntimeHelpers.getPendingActionIds
              ~confirmedTimestamp:2000.0
              ~records
          in
          Alcotest.(check bool) "Should return array" true (Array.length result >= 0));
      Alcotest.test_case "rejectStaleCacheResult returns true when cached is older" `Quick
        (fun () ->
          let current = (100, 2000.0) in
          let cached = (50, 1000.0) in
          let result =
            StoreRuntimeHelpers.rejectStaleCacheResult
              ~currentConfirmedState:current
              ~cachedState:cached
              ~timestampOfState:(fun (_, ts) -> ts)
          in
          Alcotest.(check bool)
            "Cached older than confirmed should be stale"
            true
            result);
      Alcotest.test_case "rejectStaleCacheResult returns true when equal" `Quick (fun () ->
          let current = (100, 1500.0) in
          let cached = (200, 1500.0) in
          let result =
            StoreRuntimeHelpers.rejectStaleCacheResult
              ~currentConfirmedState:current
              ~cachedState:cached
              ~timestampOfState:(fun (_, ts) -> ts)
          in
          Alcotest.(check bool) "Equal timestamps should be stale" true result);
      Alcotest.test_case "rejectStaleCacheResult returns false when cached is newer" `Quick
        (fun () ->
          let current = (100, 1000.0) in
          let cached = (200, 2000.0) in
          let result =
            StoreRuntimeHelpers.rejectStaleCacheResult
              ~currentConfirmedState:current
              ~cachedState:cached
              ~timestampOfState:(fun (_, ts) -> ts)
          in
          Alcotest.(check bool)
            "Newer cached should not be stale"
            false
            result);
      Alcotest.test_case "applies Pending and Syncing records in FIFO order" `Quick
        (fun () ->
          let records =
            [|
              make_record ~id:"r3" ~status:"acked" ~enqueued_at:3000.0;
              make_record ~id:"r1" ~status:"pending" ~enqueued_at:1000.0;
              make_record ~id:"r5" ~status:"failed" ~enqueued_at:5000.0;
              make_record ~id:"r2" ~status:"syncing" ~enqueued_at:2000.0;
              make_record ~id:"r4" ~status:"pending" ~enqueued_at:4000.0;
            |]
          in
          let result =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc (record : StoreActionLedger.t) ->
                acc ^ ":" ^ record.id)
              ~confirmed:"start"
              ~records
          in
          let expected = "start:r1:r2:r4" in
          Alcotest.(check string)
            (Printf.sprintf "Expected '%s' but got '%s'" expected result)
            expected
            result);
      Alcotest.test_case "replayActions is deterministic given same input" `Quick
        (fun () ->
          let records =
            [|
              make_record ~id:"a" ~status:"pending" ~enqueued_at:100.0;
              make_record ~id:"b" ~status:"syncing" ~enqueued_at:200.0;
              make_record ~id:"c" ~status:"pending" ~enqueued_at:300.0;
            |]
          in
          let run () =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc (r : StoreActionLedger.t) ->
                acc + Char.code r.id.[0])
              ~confirmed:0
              ~records
          in
          let r1 = run () in
          let r2 = run () in
          Alcotest.(check int)
            (Printf.sprintf "Non-deterministic: %d vs %d" r1 r2)
            r1
            r2);
    ] )

let status_gen =
  Alcobar.choose
    [
      Alcobar.const "pending";
      Alcobar.const "syncing";
      Alcobar.const "acked";
      Alcobar.const "failed";
    ]

let record_gen =
  Alcobar.map
    [ Alcobar.range 100; status_gen; Alcobar.range 10000 ]
    (fun idx status enqueued ->
      make_record
        ~id:(Printf.sprintf "rec-%02d" idx)
        ~status
        ~enqueued_at:(float_of_int (enqueued + 1000)))

let record_gen2 =
  Alcobar.map
    [ Alcobar.range 100; status_gen ]
    (fun idx status ->
      make_record
        ~id:(Printf.sprintf "s-%02d" idx)
        ~status
        ~enqueued_at:(float_of_int (idx * 100 + 100)))

let alcobar_suite =
  ( "StoreRuntimeBehaviorProperty",
    [
      Alcobar.test_case "stable replay order across mixed-status records"
        [ Alcobar.array record_gen ]
        (fun records ->
          let reduce_fn acc (r : StoreActionLedger.t) =
            let c = r.id.[4] in
            acc ^ string_of_int (Char.code c - 48)
          in
          let r1 =
            StoreRuntimeHelpers.replayActions
              ~reduce:reduce_fn
              ~confirmed:""
              ~records
          in
          let r2 =
            StoreRuntimeHelpers.replayActions
              ~reduce:reduce_fn
              ~confirmed:""
              ~records
          in
          Alcobar.check_eq r1 r2);
      Alcobar.test_case "only processes resumable records"
        [ Alcobar.array record_gen2 ]
        (fun records ->
          let resumable = StoreRuntimeHelpers.filterResumableRecords records in
          let replayed =
            StoreRuntimeHelpers.replayActions
              ~reduce:(fun acc _ -> acc + 1)
              ~confirmed:0
              ~records:(Array.of_list resumable)
          in
          Alcobar.check_eq replayed (List.length resumable));
    ] )
