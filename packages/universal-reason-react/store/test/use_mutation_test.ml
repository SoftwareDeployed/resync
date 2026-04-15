module MockMutationA = struct
  type params = string
  let dispatch text = ignore text
end

module MockMutationB = struct
  type params = int
  let dispatch _n = ignore _n
end

let suite =
  ( "UseMutation",
    [
      Alcotest.test_case "client returns dispatch function" `Quick (fun () ->
          let result = UseMutation.useMutation (module MockMutationA) in
          Alcotest.(check unit) "dispatch should be callable" () (result "test"));
      Alcotest.test_case "server returns no-op function" `Quick (fun () ->
          let result = UseMutation.useMutation (module MockMutationB) in
          Alcotest.(check unit) "no-op should be callable" () (result 42));
    ] )
