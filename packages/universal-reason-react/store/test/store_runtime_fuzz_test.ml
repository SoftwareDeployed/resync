let () =
  Alcobar.run "store-fuzz"
    [ Store_runtime_behavior_test.alcobar_suite ]
