let with_sync_registry_reset f =
  let previous = !(QueryRegistry.sync_registry_ref) in
  Fun.protect ~finally:(fun () -> QueryRegistry.sync_registry_ref := previous) f

let make_rendered_registry () =
  {
    QueryRegistry.state = QueryRegistry.Rendered;
    queries = Hashtbl.create 8;
    results = Hashtbl.create 8;
    db_connection = None;
  }

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
      Alcotest.test_case "with_serialized restores previous registry after success" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              let previous = make_rendered_registry () in
              Hashtbl.replace previous.QueryRegistry.results "previous" (`String "ok");
              QueryRegistry.sync_registry_ref := Some previous;
              let observed_loaded =
                QueryRegistry.with_serialized
                  ~jsonStr:{|{"thread:one":{"_tag":"Loaded","data":["row-one"]}}|}
                  ~f:(fun () ->
                    match !(QueryRegistry.sync_registry_ref) with
                    | None -> false
                    | Some registry ->
                        Hashtbl.mem registry.QueryRegistry.results "thread:one")
                  ()
              in
              Alcotest.(check bool)
                "temporary registry should be visible inside callback"
                true observed_loaded;
              match !(QueryRegistry.sync_registry_ref) with
              | None -> Alcotest.fail "expected previous registry to be restored"
              | Some registry ->
                  Alcotest.(check bool)
                    "previous registry restored"
                    true
                    (Hashtbl.mem registry.QueryRegistry.results "previous");
                  Alcotest.(check bool)
                    "temporary entries should not leak"
                    false
                    (Hashtbl.mem registry.QueryRegistry.results "thread:one")));
      Alcotest.test_case "with_serialized restores previous registry after exception" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              let previous = make_rendered_registry () in
              Hashtbl.replace previous.QueryRegistry.results "previous" (`String "ok");
              QueryRegistry.sync_registry_ref := Some previous;
              (try
                 QueryRegistry.with_serialized
                   ~jsonStr:{|{"thread:one":{"_tag":"Loaded","data":["row-one"]}}|}
                   ~f:(fun () ->
                     if Sys.opaque_identity true then raise (Failure "boom") else ())
                   ();
                 Alcotest.fail "expected callback exception"
               with Failure _ -> ());
              match !(QueryRegistry.sync_registry_ref) with
              | None -> Alcotest.fail "expected previous registry to be restored"
              | Some registry ->
                  Alcotest.(check bool)
                    "previous registry restored"
                    true
                    (Hashtbl.mem registry.QueryRegistry.results "previous");
                  Alcotest.(check bool)
                    "temporary entries should not leak"
                    false
                    (Hashtbl.mem registry.QueryRegistry.results "thread:one")));
    ] )
