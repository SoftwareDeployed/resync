open Test_framework
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

let init () =
  describe "StorePatch compose" (fun () ->
    test "with one decoder returns Some on match" (fun () ->
      let json =
        Json.parse
          {|{"type":"patch","table":"items","action":"INSERT","data":{"id":"1","name":"a"}}|}
      in
      let decoder = Patch.compose [ items_decoder ] in
      match decoder json with
      | Some (ItemsPatch (Crud.Upsert { id = "1"; name = "a" })) -> Passed
      | Some _ -> Failed "Wrong patch decoded"
      | None -> Failed "Should have decoded with one decoder");

    test "with multiple decoders tries them in order" (fun () ->
      let json =
        Json.parse {|{"other":"matched"}|}
      in
      let decoder = Patch.compose [ items_decoder; other_decoder ] in
      match decoder json with
      | Some (OtherPatch "matched") -> Passed
      | Some _ -> Failed "Wrong patch decoded"
      | None -> Failed "Should have decoded with second decoder");

    test "returns None when no decoder matches" (fun () ->
      let json = Json.parse {|{"unknown":"value"}|} in
      let decoder = Patch.compose [ items_decoder; other_decoder ] in
      match decoder json with
      | None -> Passed
      | Some _ -> Failed "Should return None when no decoder matches")
  );

  describe "Patch.Pg decode" (fun () ->
    test "returns None for wrong table name" (fun () ->
      let json =
        Json.parse
          {|{"type":"patch","table":"wrong","action":"INSERT","data":{"id":"1","name":"a"}}|}
      in
      match Patch.Pg.decode ~table:"items" ~decodeRow:decode_item json with
      | None -> Passed
      | Some _ -> Failed "Should return None for wrong table name");

    test "returns None for invalid JSON structure" (fun () ->
      let json = Json.parse {|{"type":"not-patch","table":"items"}|} in
      match Patch.Pg.decode ~table:"items" ~decodeRow:decode_item json with
      | None -> Passed
      | Some _ -> Failed "Should return None for invalid JSON structure");

    test "decodeAs maps Insert/Update/Delete to provided constructors" (fun () ->
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
        Some (`Delete "1") -> Passed
      | _ -> Failed "decodeAs did not map constructors correctly")
  )
