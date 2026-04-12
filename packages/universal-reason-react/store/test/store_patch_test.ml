open Store

type item = {
  id : string;
  name : string;
}

let decode_item json =
  let id =
    match Json.field json "id" with
    | Some j -> (
      match Json.tryDecode Melange_json.Primitives.string_of_json j with
      | Some s -> s
      | None -> "")
    | None -> ""
  in
  let name =
    match Json.field json "name" with
    | Some j -> (
      match Json.tryDecode Melange_json.Primitives.string_of_json j with
      | Some s -> s
      | None -> "")
    | None -> ""
  in
  { id; name }

type my_patch =
  | ItemsPatch of item Crud.patch
  | OtherPatch of string

let items_decoder json =
  match Crud.decodePatch ~table:"items" ~decodeRow:decode_item () json with
  | Some patch -> Some (ItemsPatch patch)
  | None -> None

let other_decoder json =
  match Json.field json "other" with
  | Some j -> (
    match Json.tryDecode Melange_json.Primitives.string_of_json j with
    | Some s -> Some (OtherPatch s)
    | None -> None)
  | None -> None

let patch_testable : my_patch Alcotest.testable =
  Alcotest.testable
    (fun ppf _ -> Format.fprintf ppf "<patch>")
    Stdlib.( = )

let suite =
  ( "StorePatch",
    [
      Alcotest.test_case "compose with one decoder returns Some on match" `Quick
        (fun () ->
          let json =
            Json.parse
              {|{"type":"patch","table":"items","action":"INSERT","data":{"id":"1","name":"a"}}|}
          in
          let decoder = Patch.compose [ items_decoder ] in
          match decoder json with
          | Some (ItemsPatch (Crud.Upsert { id = "1"; name = "a" })) -> ()
          | Some _ -> Alcotest.fail "Wrong patch decoded"
          | None -> Alcotest.fail "Should have decoded with one decoder");
      Alcotest.test_case "compose with multiple decoders tries them in order" `Quick
        (fun () ->
          let json = Json.parse {|{"other":"matched"}|} in
          let decoder = Patch.compose [ items_decoder; other_decoder ] in
          match decoder json with
          | Some (OtherPatch "matched") -> ()
          | Some _ -> Alcotest.fail "Wrong patch decoded"
          | None -> Alcotest.fail "Should have decoded with second decoder");
      Alcotest.test_case "compose returns None when no decoder matches" `Quick
        (fun () ->
          let json = Json.parse {|{"unknown":"value"}|} in
          let decoder = Patch.compose [ items_decoder; other_decoder ] in
          match decoder json with
          | None -> ()
          | Some _ -> Alcotest.fail "Should return None when no decoder matches");
      Alcotest.test_case "Pg.decode returns None for wrong table name" `Quick
        (fun () ->
          let json =
            Json.parse
              {|{"type":"patch","table":"wrong","action":"INSERT","data":{"id":"1","name":"a"}}|}
          in
          match Patch.Pg.decode ~table:"items" ~decodeRow:decode_item json with
          | None -> ()
          | Some _ -> Alcotest.fail "Should return None for wrong table name");
      Alcotest.test_case "Pg.decode returns None for invalid JSON structure" `Quick
        (fun () ->
          let json = Json.parse {|{"type":"not-patch","table":"items"}|} in
          match Patch.Pg.decode ~table:"items" ~decodeRow:decode_item json with
          | None -> ()
          | Some _ -> Alcotest.fail "Should return None for invalid JSON structure");
      Alcotest.test_case "decodeAs maps Insert/Update/Delete to constructors" `Quick
        (fun () ->
          let insert_json =
            Json.parse
              {|{"type":"patch","table":"items","action":"INSERT","data":{"id":"1","name":"a"}}|}
          in
          let update_json =
            Json.parse
              {|{"type":"patch","table":"items","action":"UPDATE","data":{"id":"1","name":"b"}}|}
          in
          let delete_json =
            Json.parse
              {|{"type":"patch","table":"items","action":"DELETE","id":"1"}|}
          in
          let decoder =
            Patch.Pg.decodeAs
              ~table:"items"
              ~decodeRow:decode_item
              ~insert:(fun row -> `Insert row)
              ~update:(fun row -> `Update row)
              ~delete:(fun id -> `Delete id)
              ()
          in
          let insert_result = decoder insert_json in
          let update_result = decoder update_json in
          let delete_result = decoder delete_json in
          match insert_result, update_result, delete_result with
          | Some (`Insert { id = "1"; name = "a" }),
            Some (`Update { id = "1"; name = "b" }),
            Some (`Delete "1") -> ()
          | _ -> Alcotest.fail "decodeAs did not map constructors correctly");
    ] )
