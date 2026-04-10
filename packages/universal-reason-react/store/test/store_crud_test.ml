open Test_framework

type item = {
  id : string;
  name : string;
}

let get_id i = i.id

let decode_item json =
  let id =
    match StoreJson.field json "id" with
    | Some j -> (
      match StoreJson.tryDecode Melange_json.Primitives.string_of_json j with
      | Some s -> s
      | None -> "")
    | None -> ""
  in
  let name =
    match StoreJson.field json "name" with
    | Some j -> (
      match StoreJson.tryDecode Melange_json.Primitives.string_of_json j with
      | Some s -> s
      | None -> "")
    | None -> ""
  in
  { id; name }

type config = { items : item array }

let get_items c = c.items
let set_items c items = { items }

let init () =
  describe "StoreCrud upsert" (fun () ->
    test "updates an existing item in an array" (fun () ->
      let items = [| { id = "1"; name = "old" }; { id = "2"; name = "b" } |] in
      let new_item = { id = "1"; name = "new" } in
      let result = StoreCrud.upsert ~getId:get_id items new_item in
      if Array.length result = 2 then
        match result.(0), result.(1) with
        | { id = "1"; name = "new" }, { id = "2"; name = "b" } -> Passed
        | _ -> Failed "Items not updated correctly"
      else
        Failed (Printf.sprintf "Expected length 2 but got %d" (Array.length result)));

    test "appends a new item when it doesn't exist" (fun () ->
      let items = [| { id = "1"; name = "a" } |] in
      let new_item = { id = "2"; name = "b" } in
      let result = StoreCrud.upsert ~getId:get_id items new_item in
      if Array.length result = 2 then
        match result.(0), result.(1) with
        | { id = "1"; name = "a" }, { id = "2"; name = "b" } -> Passed
        | _ -> Failed "New item not appended correctly"
      else
        Failed (Printf.sprintf "Expected length 2 but got %d" (Array.length result)))
  );

  describe "StoreCrud remove" (fun () ->
    test "deletes an item by id" (fun () ->
      let items = [| { id = "1"; name = "a" }; { id = "2"; name = "b" } |] in
      let result = StoreCrud.remove ~getId:get_id items "1" in
      if Array.length result = 1 then
        match result.(0) with
        | { id = "2"; name = "b" } -> Passed
        | _ -> Failed "Wrong item remaining"
      else
        Failed (Printf.sprintf "Expected length 1 but got %d" (Array.length result)))
  );

  describe "StoreCrud updateOfPatch" (fun () ->
    test "Upsert applies upsert to config" (fun () ->
      let config = { items = [| { id = "1"; name = "a" } |] } in
      let patch = StoreCrud.Upsert { id = "2"; name = "b" } in
      let updater =
        StoreCrud.updateOfPatch
          ~getId:get_id
          ~getItems:get_items
          ~setItems:set_items
          patch
      in
      let result = updater config in
      if Array.length result.items = 2 then
        match result.items.(0), result.items.(1) with
        | { id = "1"; name = "a" }, { id = "2"; name = "b" } -> Passed
        | _ -> Failed "Config not updated correctly for Upsert"
      else
        Failed (Printf.sprintf "Expected length 2 but got %d" (Array.length result.items)));

    test "Delete applies remove to config" (fun () ->
      let config = { items = [| { id = "1"; name = "a" }; { id = "2"; name = "b" } |] } in
      let patch = StoreCrud.Delete "1" in
      let updater =
        StoreCrud.updateOfPatch
          ~getId:get_id
          ~getItems:get_items
          ~setItems:set_items
          patch
      in
      let result = updater config in
      if Array.length result.items = 1 then
        match result.items.(0) with
        | { id = "2"; name = "b" } -> Passed
        | _ -> Failed "Config not updated correctly for Delete"
      else
        Failed (Printf.sprintf "Expected length 1 but got %d" (Array.length result.items)))
  );

  describe "StoreCrud decodePatch" (fun () ->
    test "decodes a valid JSON patch into Upsert (INSERT)" (fun () ->
      let json =
        StoreJson.parse
          {|{"type":"patch","table":"items","action":"INSERT","data":{"id":"1","name":"a"}}|}
      in
      let decoder = StoreCrud.decodePatch ~table:"items" ~decodeRow:decode_item () in
      match decoder json with
      | Some (StoreCrud.Upsert { id = "1"; name = "a" }) -> Passed
      | Some _ -> Failed "Decoded to wrong patch type"
      | None -> Failed "Failed to decode valid INSERT patch");

    test "decodes a valid JSON patch into Upsert (UPDATE)" (fun () ->
      let json =
        StoreJson.parse
          {|{"type":"patch","table":"items","action":"UPDATE","data":{"id":"1","name":"updated"}}|}
      in
      let decoder = StoreCrud.decodePatch ~table:"items" ~decodeRow:decode_item () in
      match decoder json with
      | Some (StoreCrud.Upsert { id = "1"; name = "updated" }) -> Passed
      | Some _ -> Failed "Decoded to wrong patch type"
      | None -> Failed "Failed to decode valid UPDATE patch");

    test "decodes a valid JSON delete patch into Delete" (fun () ->
      let json =
        StoreJson.parse
          {|{"type":"patch","table":"items","action":"DELETE","id":"1"}|}
      in
      let decoder = StoreCrud.decodePatch ~table:"items" ~decodeRow:decode_item () in
      match decoder json with
      | Some (StoreCrud.Delete "1") -> Passed
      | Some _ -> Failed "Decoded to wrong patch type"
      | None -> Failed "Failed to decode valid DELETE patch");

    test "returns None for mismatched table" (fun () ->
      let json =
        StoreJson.parse
          {|{"type":"patch","table":"other","action":"INSERT","data":{"id":"1","name":"a"}}|}
      in
      let decoder = StoreCrud.decodePatch ~table:"items" ~decodeRow:decode_item () in
      match decoder json with
      | None -> Passed
      | Some _ -> Failed "Should return None for mismatched table")
  )
