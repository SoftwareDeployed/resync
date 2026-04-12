let () =
  Alcotest.run "store"
    [ Store_events_test.suite
    ; Store_action_ledger_test.suite
    ; Store_events_behavior_test.suite
    ; Store_runtime_behavior_test.suite
    ; Store_crud_test.suite
    ; Store_patch_test.suite
    ; Store_source_test.suite
    ; Store_cache_test.suite
    ]
