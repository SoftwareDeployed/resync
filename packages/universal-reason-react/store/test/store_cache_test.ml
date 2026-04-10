open Test_framework

module TestNoCacheSchema = struct
  type state = int
  type action = string
end

module NC = StoreCache.NoCache (TestNoCacheSchema)

let init () =
  describe "StoreCache.NoCache" (fun () ->
    test "getState returns None" (fun () ->
      let result : int StoreCache.state_record option ref = ref None in
      let resolved = ref false in
      let _ = Js.Promise.then_ (fun v ->
        result := v;
        resolved := true;
        Js.Promise.resolve ()
      ) (NC.getState ~storeName:"test" ~scopeKey:"k" ()) in
      if not !resolved then Failed "Promise did not resolve"
      else
        match !result with
        | None -> Passed
        | Some _ -> Failed "Expected None"
    );

    test "setState resolves without error" (fun () ->
      let record = ({ scopeKey = "k"; state = 42; timestamp = 0.0 } : int StoreCache.state_record) in
      let resolved = ref false in
      let _ = Js.Promise.then_ (fun () ->
        resolved := true;
        Js.Promise.resolve ()
      ) (NC.setState ~storeName:"test" record) in
      if !resolved then Passed else Failed "setState did not resolve"
    );

    test "getActionsByScope returns empty array" (fun () ->
      let result : string StoreCache.action_record array ref = ref [||] in
      let resolved = ref false in
      let _ = Js.Promise.then_ (fun v ->
        result := v;
        resolved := true;
        Js.Promise.resolve ()
      ) (NC.getActionsByScope ~storeName:"test" ~scopeKey:"k" ()) in
      if not !resolved then Failed "Promise did not resolve"
      else if Array.length !result = 0 then Passed
      else Failed "Expected empty array"
    )
  )
