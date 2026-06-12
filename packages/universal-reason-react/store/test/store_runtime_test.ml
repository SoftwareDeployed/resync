let () =
  Alcotest.run "store"
    [ Store_events_test.suite
    ; Store_action_ledger_test.suite
    ; Store_events_behavior_test.suite
    ; Store_runtime_behavior_test.suite
    ; Store_runtime_lifecycle_test.suite
    ; Store_crud_test.suite
    ; Store_patch_test.suite
    ; Store_source_test.suite
    ; Store_cache_test.suite
    ; Realtime_client_test.suite
    ; Query_registry_test.suite
    ; Use_query_test.suite
    ; Use_mutation_test.suite
    ]
