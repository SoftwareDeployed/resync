open Store

type item = {
  id : string;
  name : string;
}

let get_id i = i.id

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

type config = { items : item array }

let get_items c = c.items
let set_items c items = { items }

let item_testable =
  Alcotest.testable
    (fun ppf { id; name } -> Format.fprintf ppf "{ id=%S; name=%S }" id name)
    Stdlib.( = )

let config_testable =
  Alcotest.testable
    (fun ppf { items } -> Format.fprintf ppf "{ items=[|%a|] }"
       (Format.pp_print_list ~pp_sep:(fun ppf () -> Format.fprintf ppf "; ")
         (fun ppf it -> Format.fprintf ppf "{ id=%S; name=%S }" it.id it.name))
       (Array.to_list items))
    Stdlib.( = )

let suite =
  ( "StoreCrud",
    [
      Alcotest.test_case "upsert updates an existing item in an array" `Quick
        (fun () ->
          let items = [| { id = "1"; name = "old" }; { id = "2"; name = "b" } |] in
          let new_item = { id = "1"; name = "new" } in
          let result = Crud.upsert ~getId:get_id items new_item in
          Alcotest.(check int)
            (Printf.sprintf "Expected length 2 but got %d" (Array.length result))
            2
            (Array.length result);
          match result.(0), result.(1) with
          | { id = "1"; name = "new" }, { id = "2"; name = "b" } -> ()
          | _ -> Alcotest.fail "Items not updated correctly");
      Alcotest.test_case "upsert appends a new item when it doesn't exist" `Quick
        (fun () ->
          let items = [| { id = "1"; name = "a" } |] in
          let new_item = { id = "2"; name = "b" } in
          let result = Crud.upsert ~getId:get_id items new_item in
          Alcotest.(check int)
            (Printf.sprintf "Expected length 2 but got %d" (Array.length result))
            2
            (Array.length result);
          match result.(0), result.(1) with
          | { id = "1"; name = "a" }, { id = "2"; name = "b" } -> ()
          | _ -> Alcotest.fail "New item not appended correctly");
      Alcotest.test_case "remove deletes an item by id" `Quick (fun () ->
          let items =
            [| { id = "1"; name = "a" }; { id = "2"; name = "b" } |]
          in
          let result = Crud.remove ~getId:get_id items "1" in
          Alcotest.(check int)
            (Printf.sprintf "Expected length 1 but got %d" (Array.length result))
            1
            (Array.length result);
          match result.(0) with
          | { id = "2"; name = "b" } -> ()
          | _ -> Alcotest.fail "Wrong item remaining");
      Alcotest.test_case "Upsert applies upsert to config" `Quick (fun () ->
          let config = { items = [| { id = "1"; name = "a" } |] } in
          let patch = Crud.Upsert { id = "2"; name = "b" } in
          let updater =
            Crud.updateOfPatch ~getId:get_id ~getItems:get_items ~setItems:set_items
              patch
          in
          let result = updater config in
          Alcotest.(check int)
            (Printf.sprintf "Expected length 2 but got %d" (Array.length result.items))
            2
            (Array.length result.items);
          match result.items.(0), result.items.(1) with
          | { id = "1"; name = "a" }, { id = "2"; name = "b" } -> ()
          | _ -> Alcotest.fail "Config not updated correctly for Upsert");
      Alcotest.test_case "Delete applies remove to config" `Quick (fun () ->
          let config =
            { items = [| { id = "1"; name = "a" }; { id = "2"; name = "b" } |] }
          in
          let patch = Crud.Delete "1" in
          let updater =
            Crud.updateOfPatch ~getId:get_id ~getItems:get_items ~setItems:set_items
              patch
          in
          let result = updater config in
          Alcotest.(check int)
            (Printf.sprintf "Expected length 1 but got %d" (Array.length result.items))
            1
            (Array.length result.items);
          match result.items.(0) with
          | { id = "2"; name = "b" } -> ()
          | _ -> Alcotest.fail "Config not updated correctly for Delete");
      Alcotest.test_case "decodes a valid JSON patch into Upsert (INSERT)" `Quick
        (fun () ->
          let json =
            Json.parse
              {|{"type":"patch","table":"items","action":"INSERT","data":{"id":"1","name":"a"}}|}
          in
          let decoder =
            Crud.decodePatch ~table:"items" ~decodeRow:decode_item ()
          in
          match decoder json with
          | Some (Crud.Upsert { id = "1"; name = "a" }) -> ()
          | Some _ -> Alcotest.fail "Decoded to wrong patch type"
          | None -> Alcotest.fail "Failed to decode valid INSERT patch");
      Alcotest.test_case "decodes a valid JSON patch into Upsert (UPDATE)" `Quick
        (fun () ->
          let json =
            Json.parse
              {|{"type":"patch","table":"items","action":"UPDATE","data":{"id":"1","name":"updated"}}|}
          in
          let decoder =
            Crud.decodePatch ~table:"items" ~decodeRow:decode_item ()
          in
          match decoder json with
          | Some (Crud.Upsert { id = "1"; name = "updated" }) -> ()
          | Some _ -> Alcotest.fail "Decoded to wrong patch type"
          | None -> Alcotest.fail "Failed to decode valid UPDATE patch");
      Alcotest.test_case "decodes a valid JSON delete patch into Delete" `Quick
        (fun () ->
          let json =
            Json.parse
              {|{"type":"patch","table":"items","action":"DELETE","id":"1"}|}
          in
          let decoder =
            Crud.decodePatch ~table:"items" ~decodeRow:decode_item ()
          in
          match decoder json with
          | Some (Crud.Delete "1") -> ()
          | Some _ -> Alcotest.fail "Decoded to wrong patch type"
          | None -> Alcotest.fail "Failed to decode valid DELETE patch");
      Alcotest.test_case "returns None for mismatched table" `Quick (fun () ->
          let json =
            Json.parse
              {|{"type":"patch","table":"other","action":"INSERT","data":{"id":"1","name":"a"}}|}
          in
          let decoder =
            Crud.decodePatch ~table:"items" ~decodeRow:decode_item ()
          in
          match decoder json with
          | None -> ()
          | Some _ -> Alcotest.fail "Should return None for mismatched table");
    ] )
