let with_sync_registry_reset f =
  let previous = !(QueryRegistry.sync_registry_ref) in
  Fun.protect ~finally:(fun () -> QueryRegistry.sync_registry_ref := previous) f

let has_result key = Option.is_some (QueryRegistry.find_result ~key)
let has_error key = Option.is_some (QueryRegistry.find_error ~key)

let result_json_string key =
  match QueryRegistry.find_result ~key with
  | Some json -> Yojson.Safe.to_string json
  | None -> Alcotest.fail ("expected result for " ^ key)

let error_message key =
  match QueryRegistry.find_error ~key with
  | Some message -> message
  | None -> Alcotest.fail ("expected error for " ^ key)

let decode_string = function
  | `String value -> value
  | _ -> failwith "expected string row"

let register_cached_string_query key =
  QueryRegistry.register_query ~key ~channel:"thread" ~params:() ~sql:""
    ~execute:(fun _db -> Lwt.return (Ok (`List [])))
    ~decode:decode_string

let find_loaded_result key results =
  let found = ref None in
  for i = 0 to Array.length results - 1 do
    let result = results.(i) in
    if result.QueryRegistryTypes.key = key then found := Some result
  done;
  match !found with
  | Some result -> result
  | None -> Alcotest.fail ("expected loaded result for " ^ key)

let setup_previous_registry () =
  QueryRegistry.setup_registry_from_json
    ~jsonStr:{|{"previous":{"_tag":"Loaded","data":"ok"}}|}

let suite =
  ( "QueryRegistry",
    [
      Alcotest.test_case "query keys round-trip colon channels and params" `Quick
        (fun () ->
          let key =
            QueryRegistryTypes.makeKey ~channel:"thread:abc"
              ~paramsHash:"messages:latest"
          in
          Alcotest.(check bool)
            "versioned key prefix"
            true
            (String.starts_with ~prefix:"v1:" key);
          Alcotest.(check string)
            "length-prefixed channel"
            "thread:abc"
            (QueryRegistryTypes.channelOfKey key);
          Alcotest.(check string)
            "legacy channel"
            "thread:abc"
            (QueryRegistryTypes.channelOfKey "thread:abc:messages");
          Alcotest.(check bool)
            "unprefixed length parser rejects legacy key"
            true
            (Option.is_none
               (QueryRegistryTypes.channelOfLengthPrefixedKey "2:ab:x"));
          Alcotest.(check string)
            "numeric legacy channel"
            "2:ab"
            (QueryRegistryTypes.channelOfKey "2:ab:x"));
      Alcotest.test_case "setup_registry_from_json hydrates loaded cache data" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              QueryRegistry.setup_registry_from_json
                ~jsonStr:
                  {|{"thread:one":{"_tag":"Loaded","data":["row-one"]},"thread:error":{"_tag":"Error","message":"bad"},"thread:missing":{"_tag":"Loaded"}}|};
              Alcotest.(check bool)
                "loaded entry should hydrate"
                true
                (has_result "thread:one");
              Alcotest.(check string)
                "loaded data"
                {|["row-one"]|}
                (result_json_string "thread:one");
              Alcotest.(check bool)
                "error entry has no data payload"
                false
                (has_result "thread:error");
              Alcotest.(check bool)
                "error entry should hydrate as error"
                true
                (has_error "thread:error");
              Alcotest.(check string)
                "error message"
                "bad"
                (error_message "thread:error");
              Alcotest.(check bool)
                "missing data entry should be ignored"
                false
                (has_result "thread:missing")));
      Alcotest.test_case "get_loaded_results returns decoded JSON row arrays" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              let colon_key =
                QueryRegistryTypes.makeKey ~channel:"thread:one"
                  ~paramsHash:"messages:all"
              in
              QueryRegistry.setup_registry_from_json
                ~jsonStr:
                  (Yojson.Safe.to_string
                     (`Assoc
                       [
                         ( colon_key,
                           `Assoc
                             [
                               ("_tag", `String "Loaded");
                               ( "data",
                                 `List [ `String "row-one"; `String "row-two" ]
                               );
                             ] );
                         ( "other:two",
                           `Assoc
                             [
                               ("_tag", `String "Loaded");
                               ("data", `List [ `String "other-row" ]);
                             ] );
                         ( "thread:error",
                           `Assoc
                             [
                               ("_tag", `String "Error");
                               ("message", `String "bad");
                             ] );
                       ]));
              let results = QueryRegistry.get_loaded_results () in
              Alcotest.(check int)
                "loaded result count"
                2
                (Array.length results);
              Alcotest.(check string)
                "first loaded result follows registry order"
                colon_key
                results.(0).QueryRegistryTypes.key;
              Alcotest.(check string)
                "second loaded result follows registry order"
                "other:two"
                results.(1).QueryRegistryTypes.key;
              let result = find_loaded_result colon_key results in
              Alcotest.(check string) "key" colon_key result.QueryRegistryTypes.key;
              Alcotest.(check string) "channel" "thread:one" result.QueryRegistryTypes.channel;
              Alcotest.(check int)
                "row count"
                2
                (Array.length result.QueryRegistryTypes.rows);
              Alcotest.(check string)
                "first row"
                "row-one"
                (decode_string result.QueryRegistryTypes.rows.(0));
              Alcotest.(check string)
                "second row"
                "row-two"
                (decode_string result.QueryRegistryTypes.rows.(1));
              let otherResult = find_loaded_result "other:two" results in
              Alcotest.(check string)
                "other channel"
                "other"
                otherResult.QueryRegistryTypes.channel;
              Alcotest.(check string)
                "other row"
                "other-row"
                (decode_string otherResult.QueryRegistryTypes.rows.(0))));
      Alcotest.test_case "register_query returns cached rows as an array" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              QueryRegistry.setup_registry_from_json
                ~jsonStr:
                  {|{"thread:one":{"_tag":"Loaded","data":["row-one","row-two"]}}|};
              match register_cached_string_query "thread:one" with
              | Some rows ->
                  Alcotest.(check int) "row count" 2 (Array.length rows);
                  Alcotest.(check string) "first row" "row-one" rows.(0);
                  Alcotest.(check string) "second row" "row-two" rows.(1)
              | None ->
                  Alcotest.fail
                    "cached loaded rows should decode through register_query"));
      Alcotest.test_case "setup_registry_from_json ignores non-loaded data payloads" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              QueryRegistry.setup_registry_from_json
                ~jsonStr:
                  {|{"thread:error-data":{"_tag":"Error","message":"bad","data":["bad"]},"thread:loading-data":{"_tag":"Loading","data":["bad"]}}|};
              Alcotest.(check bool)
                "error result with data should not hydrate"
                false
                (has_result "thread:error-data");
              Alcotest.(check string)
                "error result should preserve message"
                "bad"
                (error_message "thread:error-data");
              Alcotest.(check bool)
                "loading result with data should not hydrate"
                false
                (has_result "thread:loading-data")));
      Alcotest.test_case "with_serialized restores previous registry after success" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              setup_previous_registry ();
              let observed_loaded =
                QueryRegistry.with_serialized
                  ~jsonStr:{|{"thread:one":{"_tag":"Loaded","data":["row-one"]}}|}
                  ~f:(fun () -> has_result "thread:one")
                  ()
              in
              Alcotest.(check bool)
                "temporary registry should be visible inside callback"
                true observed_loaded;
              Alcotest.(check bool)
                "previous registry restored"
                true
                (has_result "previous");
              Alcotest.(check bool)
                "temporary entries should not leak"
                false
                (has_result "thread:one")));
      Alcotest.test_case "with_serialized_lwt scopes cache to Lwt context" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              setup_previous_registry ();
              let observed_loaded, observed_previous =
                Lwt_main.run
                  (QueryRegistry.with_serialized_lwt
                     ~jsonStr:
                       {|{"thread:one":{"_tag":"Loaded","data":["row-one"]}}|}
                     ~f:(fun () ->
                       Lwt.return
                         (has_result "thread:one", has_result "previous"))
                     ())
              in
              Alcotest.(check bool)
                "scoped registry should be visible inside callback"
                true observed_loaded;
              Alcotest.(check bool)
                "sync registry should be hidden inside Lwt scope"
                false observed_previous;
              Alcotest.(check bool)
                "previous registry restored after Lwt scope"
                true
                (has_result "previous");
              Alcotest.(check bool)
                "scoped entries should not leak"
                false
                (has_result "thread:one")));
      Alcotest.test_case "with_serialized restores previous registry after exception" `Quick
        (fun () ->
          with_sync_registry_reset (fun () ->
              setup_previous_registry ();
              (try
                 QueryRegistry.with_serialized
                   ~jsonStr:{|{"thread:one":{"_tag":"Loaded","data":["row-one"]}}|}
                   ~f:(fun () ->
                     if Sys.opaque_identity true then raise (Failure "boom") else ())
                   ();
                 Alcotest.fail "expected callback exception"
               with Failure _ -> ());
              Alcotest.(check bool)
                "previous registry restored"
                true
                (has_result "previous");
              Alcotest.(check bool)
                "temporary entries should not leak"
                false
                (has_result "thread:one")));
    ] )
