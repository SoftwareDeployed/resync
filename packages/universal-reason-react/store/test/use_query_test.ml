module DecodeFailingQuery = struct
  type params = string
  type row = string

  let channel _params = "decode-failing"
  let paramsHash params = params
  let decodeRow _json = failwith "bad row"
  let row_to_json row = Melange_json.Primitives.string_to_json row
  let execute _db _params = Lwt.return (Ok [||])
end

module StringQuery = struct
  type params = string
  type row = string

  let channel _params = "strings"
  let paramsHash params = params
  let decodeRow = Melange_json.Primitives.string_of_json
  let row_to_json = Melange_json.Primitives.string_to_json
  let execute _db _params = Lwt.return (Ok [||])
end

let with_sync_registry ?(key = "decode-failing:params") json f =
  let previous = !(QueryRegistry.sync_registry_ref) in
  let jsonStr =
    Yojson.Safe.to_string
      (`Assoc
        [
          ( key,
            `Assoc [ ("_tag", `String "Loaded"); ("data", json) ] );
        ])
  in
  QueryRegistry.setup_registry_from_json ~jsonStr;
  Fun.protect ~finally:(fun () -> QueryRegistry.sync_registry_ref := previous) f

let with_error_registry ?(key = "strings:params") message f =
  let previous = !(QueryRegistry.sync_registry_ref) in
  let jsonStr =
    Yojson.Safe.to_string
      (`Assoc
        [
          ( key,
            `Assoc [ ("_tag", `String "Error"); ("message", `String message) ]
          );
        ])
  in
  QueryRegistry.setup_registry_from_json ~jsonStr;
  Fun.protect ~finally:(fun () -> QueryRegistry.sync_registry_ref := previous) f

let suite =
  ( "UseQuery",
    [
      Alcotest.test_case "server cached array rows decode to Loaded" `Quick
        (fun () ->
          with_sync_registry
            ~key:"strings:params"
            (`List [ `String "first"; `String "second" ])
            (fun () ->
              let result = UseQuery.useQuery (module StringQuery) "params" () in
              match result.data with
              | QueryRegistryTypes.Loaded rows ->
                  Alcotest.(check int) "row count" 2 (Array.length rows);
                  Alcotest.(check string) "first row" "first" rows.(0);
                  Alcotest.(check string) "second row" "second" rows.(1)
              | QueryRegistryTypes.Loading ->
                  Alcotest.fail "cached array rows should not stay Loading"
              | QueryRegistryTypes.Error msg ->
                  Alcotest.fail ("cached array rows should decode: " ^ msg)));
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
      Alcotest.test_case "server cached query error returns Error" `Quick
        (fun () ->
          with_error_registry "database unavailable" (fun () ->
              let result = UseQuery.useQuery (module StringQuery) "params" () in
              match result.data with
              | QueryRegistryTypes.Error msg ->
                  Alcotest.(check string)
                    "query error"
                    "database unavailable"
                    msg
              | QueryRegistryTypes.Loading ->
                  Alcotest.fail "cached query error should not stay Loading"
              | QueryRegistryTypes.Loaded _ ->
                  Alcotest.fail "cached query error should not return Loaded"));
    ] )
