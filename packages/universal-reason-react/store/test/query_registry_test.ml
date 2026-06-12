let with_sync_registry_reset f =
  let previous = !(QueryRegistry.sync_registry_ref) in
  Fun.protect ~finally:(fun () -> QueryRegistry.sync_registry_ref := previous) f

let suite =
  ( "QueryRegistry",
    [
      Alcotest.test_case "setup_registry_from_json hydrates loaded cache data" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              QueryRegistry.setup_registry_from_json
                ~jsonStr:
                  {|{"thread:one":{"_tag":"Loaded","data":["row-one"]},"thread:error":{"_tag":"Error","message":"bad"},"thread:missing":{"_tag":"Loaded"}}|};
              match !(QueryRegistry.sync_registry_ref) with
              | None -> Alcotest.fail "expected sync registry"
              | Some registry ->
                  Alcotest.(check bool)
                    "loaded entry should hydrate"
                    true
                    (Hashtbl.mem registry.QueryRegistry.results "thread:one");
                  Alcotest.(check string)
                    "loaded data"
                    {|["row-one"]|}
                    (Yojson.Safe.to_string
                       (Hashtbl.find registry.QueryRegistry.results "thread:one"));
                  Alcotest.(check bool)
                    "error entry has no data payload"
                    false
                    (Hashtbl.mem registry.QueryRegistry.results "thread:error");
                  Alcotest.(check bool)
                    "missing data entry should be ignored"
                    false
                    (Hashtbl.mem registry.QueryRegistry.results "thread:missing")));
    ] )
