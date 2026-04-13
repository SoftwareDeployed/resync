let () = Alcotest.run "dream-middleware" [ Middleware_behavior_test.suite; Action_store_test.suite ]
