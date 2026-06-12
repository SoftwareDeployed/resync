let check_status label expected_pending expected_idle lifecycle =
  let status = StoreRuntimeLifecycle.status lifecycle in
  Alcotest.(check int)
    (label ^ " pending actions")
    expected_pending
    status.StoreRuntimeTypes.pendingActions;
  Alcotest.(check bool)
    (label ^ " idle")
    expected_idle
    status.StoreRuntimeTypes.idle

let suite =
  ( "StoreRuntimeLifecycle",
    [
      Alcotest.test_case "pending actions are tracked by id" `Quick (fun () ->
          let lifecycle = StoreRuntimeLifecycle.make ~storeName:"test" () in
          StoreRuntimeLifecycle.trackBoot lifecycle (Js.Promise.resolve ())
          |> ignore;
          StoreRuntimeLifecycle.markActionPending lifecycle "a";
          StoreRuntimeLifecycle.markActionPending lifecycle "b";
          check_status "two pending" 2 false lifecycle;
          StoreRuntimeLifecycle.markActionSettled lifecycle "a";
          check_status "one pending" 1 false lifecycle;
          StoreRuntimeLifecycle.markActionSettled lifecycle "a";
          check_status "duplicate settle" 1 false lifecycle;
          StoreRuntimeLifecycle.markActionSettled lifecycle "missing";
          check_status "unknown settle" 1 false lifecycle;
          StoreRuntimeLifecycle.markActionSettled lifecycle "b";
          check_status "all settled" 0 true lifecycle);
      Alcotest.test_case "duplicate pending id does not inflate status" `Quick
        (fun () ->
          let lifecycle = StoreRuntimeLifecycle.make ~storeName:"test" () in
          StoreRuntimeLifecycle.trackBoot lifecycle (Js.Promise.resolve ())
          |> ignore;
          StoreRuntimeLifecycle.markActionPending lifecycle "a";
          StoreRuntimeLifecycle.markActionPending lifecycle "a";
          check_status "duplicate pending" 1 false lifecycle;
          StoreRuntimeLifecycle.markActionSettled lifecycle "a";
          check_status "settled once" 0 true lifecycle);
    ] )
