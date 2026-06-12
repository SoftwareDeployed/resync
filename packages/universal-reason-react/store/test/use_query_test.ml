module DecodeFailingQuery = struct
  type params = string
  type row = string

  let channel _params = "decode-failing"
  let paramsHash params = params
  let decodeRow _json = failwith "bad row"
  let row_to_json row = Melange_json.Primitives.string_to_json row
  let execute _db _params = Lwt.return (Ok [||])
end

let with_sync_registry json f =
  let results = Hashtbl.create 1 in
  Hashtbl.replace results "decode-failing:params" json;
  let registry =
    {
      QueryRegistry.state = QueryRegistry.Rendered;
      queries = Hashtbl.create 1;
      results;
      db_connection = None;
    }
  in
  let previous = !(QueryRegistry.sync_registry_ref) in
  QueryRegistry.sync_registry_ref := Some registry;
  Fun.protect ~finally:(fun () -> QueryRegistry.sync_registry_ref := previous) f

let suite =
  ( "UseQuery",
    [
      Alcotest.test_case "server decode failure returns Error" `Quick (fun () ->
          with_sync_registry (`String "not a row") (fun () ->
              let result =
                UseQuery.useQuery (module DecodeFailingQuery) "params" ()
              in
              match result.data with
              | QueryRegistryTypes.Error msg ->
                  Alcotest.(check string)
                    "decode error"
                    "Failed to decode query result"
                    msg
              | QueryRegistryTypes.Loading ->
                  Alcotest.fail "decode failure should not stay Loading"
              | QueryRegistryTypes.Loaded _ ->
                  Alcotest.fail "decode failure should not return Loaded"));
    ] )
