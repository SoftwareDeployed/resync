module MockMutation = struct
  type params = string
end

let promise_resolves promise =
  let resolved = ref false in
  let _ =
    Js.Promise.then_
      (fun () ->
        resolved := true;
        Js.Promise.resolve ())
      promise
  in
  !resolved

let suite =
  ( "UseMutation",
    [
      Alcotest.test_case "server dispatch resolves without side effects" `Quick
        (fun () ->
          let result = UseMutation.make (module MockMutation) () in
          Alcotest.(check bool)
            "dispatch promise should resolve"
            true
            (promise_resolves (result.dispatch "test")));
      Alcotest.test_case "server mutate alias matches dispatch behavior" `Quick
        (fun () ->
          let result = UseMutation.make (module MockMutation) () in
          Alcotest.(check bool)
            "mutate promise should resolve"
            true
            (promise_resolves (result.mutate "test")));
      Alcotest.test_case "server mutation function resolves" `Quick
        (fun () ->
          let mutate = UseMutation.makeFn (module MockMutation) () in
          Alcotest.(check bool)
            "mutation function promise should resolve"
            true
            (promise_resolves (mutate "test")));
      Alcotest.test_case "low-level useMutation is callable" `Quick
        (fun () ->
          let mutate = Hooks.useMutation (module MockMutation) () in
          Alcotest.(check bool)
            "useMutation should return a mutation function"
            true
            (promise_resolves (mutate "test")));
      Alcotest.test_case "low-level useMutationFn remains callable alias" `Quick
        (fun () ->
          let mutate = Hooks.useMutationFn (module MockMutation) () in
          Alcotest.(check bool)
            "useMutationFn should return a mutation function"
            true
            (promise_resolves (mutate "test")));
      Alcotest.test_case "low-level useMutationResult returns handle" `Quick
        (fun () ->
          let result = Hooks.useMutationResult (module MockMutation) () in
          Alcotest.(check bool)
            "useMutationResult dispatch promise should resolve"
            true
            (promise_resolves (result.dispatch "test")));
    ] )
