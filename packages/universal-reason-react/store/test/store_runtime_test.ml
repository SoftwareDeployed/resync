let () =
  Store_events_test.init ();
  Store_action_ledger_test.init ();
  Store_events_behavior_test.init ();
  Store_runtime_behavior_test.init ();
  Store_crud_test.init ();
  Store_patch_test.init ();
  Store_source_test.init ();
  Test_framework.run_all () |> exit
