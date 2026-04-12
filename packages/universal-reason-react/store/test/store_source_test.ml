let suite =
  ( "StoreSource",
    [
      Alcotest.test_case "make returns correct initial value" `Quick (fun () ->
          let source = StoreSource.make 42 in
          Alcotest.(check int) "Initial value incorrect" 42 (source.get ()));
      Alcotest.test_case "get returns the set value after set" `Quick (fun () ->
          let source = StoreSource.make 0 in
          source.set 100;
          Alcotest.(check int) "Get did not return set value" 100 (source.get ()));
      Alcotest.test_case "revision increments after each set" `Quick (fun () ->
          let source = StoreSource.make "a" in
          let r0 = source.revision () in
          source.set "b";
          let r1 = source.revision () in
          source.set "c";
          let r2 = source.revision () in
          Alcotest.(check bool)
            (Printf.sprintf "Revisions: %d, %d, %d" r0 r1 r2)
            true
            (r0 = 0 && r1 = 1 && r2 = 2));
      Alcotest.test_case "update applies the reducer and increments revision" `Quick
        (fun () ->
          let source = StoreSource.make 10 in
          let r0 = source.revision () in
          source.update (fun x -> x * 2);
          let r1 = source.revision () in
          Alcotest.(check bool)
            "Update did not apply reducer or increment revision correctly"
            true
            (source.get () = 20 && r0 = 0 && r1 = 1));
      Alcotest.test_case "afterSet callback is invoked on set" `Quick (fun () ->
          let called = ref false in
          let received = ref None in
          let after_set v =
            called := true;
            received := Some v
          in
          let source = StoreSource.make ~afterSet:after_set 0 in
          source.set 42;
          Alcotest.(check bool)
            "afterSet callback was not invoked on set"
            true
            (!called && !received = Some 42));
    ] )
