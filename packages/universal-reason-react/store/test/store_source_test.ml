open Test_framework

let init () =
  describe "StoreSource make" (fun () ->
    test "returns correct initial value" (fun () ->
      let source = StoreSource.make 42 in
      if source.get () = 42 then Passed else Failed "Initial value incorrect")
  );

  describe "StoreSource get and set" (fun () ->
    test "get returns the set value after set" (fun () ->
      let source = StoreSource.make 0 in
      source.set 100;
      if source.get () = 100 then Passed else Failed "Get did not return set value")
  );

  describe "StoreSource revision" (fun () ->
    test "increments after each set" (fun () ->
      let source = StoreSource.make "a" in
      let r0 = source.revision () in
      source.set "b";
      let r1 = source.revision () in
      source.set "c";
      let r2 = source.revision () in
      if r0 = 0 && r1 = 1 && r2 = 2 then Passed
      else Failed (Printf.sprintf "Revisions: %d, %d, %d" r0 r1 r2))
  );

  describe "StoreSource update" (fun () ->
    test "applies the reducer and increments revision" (fun () ->
      let source = StoreSource.make 10 in
      let r0 = source.revision () in
      source.update (fun x -> x * 2);
      let r1 = source.revision () in
      if source.get () = 20 && r0 = 0 && r1 = 1 then Passed
      else Failed "Update did not apply reducer or increment revision correctly")
  );

  describe "StoreSource afterSet" (fun () ->
    test "callback is invoked on set" (fun () ->
      let called = ref false in
      let received = ref None in
      let after_set v =
        called := true;
        received := Some v
      in
      let source = StoreSource.make ~afterSet:after_set 0 in
      source.set 42;
      if !called && !received = Some 42 then Passed
      else Failed "afterSet callback was not invoked on set")
  )
