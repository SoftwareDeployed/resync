open Store

module TestNoCacheSchema = struct
  type state = int
  type action = string
end

module NC = StoreCache.NoCache (TestNoCacheSchema)

let suite =
  ( "StoreCache.NoCache",
    [
      Alcotest.test_case "getState returns None" `Quick (fun () ->
          let result : int StoreCache.state_record option ref = ref None in
          let resolved = ref false in
          let _ =
            Js.Promise.then_
              (fun v ->
                result := v;
                resolved := true;
                Js.Promise.resolve ())
              (NC.getState ~storeName:"test" ~scopeKey:"k" ())
          in
          if not !resolved then
            Alcotest.fail "Promise did not resolve"
          else
            match !result with
            | None -> ()
            | Some _ -> Alcotest.fail "Expected None");
      Alcotest.test_case "setState resolves without error" `Quick (fun () ->
          let record =
            ({ scopeKey = "k"; state = 42; timestamp = 0.0 } : int StoreCache.state_record)
          in
          let resolved = ref false in
          let _ =
            Js.Promise.then_
              (fun () ->
                resolved := true;
                Js.Promise.resolve ())
              (NC.setState ~storeName:"test" record)
          in
          if not !resolved then
            Alcotest.fail "setState did not resolve"
          else
            ());
      Alcotest.test_case "getActionsByScope returns empty array" `Quick (fun () ->
          let result : string StoreCache.action_record array ref = ref [||] in
          let resolved = ref false in
          let _ =
            Js.Promise.then_
              (fun v ->
                result := v;
                resolved := true;
                Js.Promise.resolve ())
              (NC.getActionsByScope ~storeName:"test" ~scopeKey:"k" ())
          in
          if not !resolved then
            Alcotest.fail "Promise did not resolve"
          else if Array.length !result = 0 then
            ()
          else
            Alcotest.fail "Expected empty array");
    ] )
