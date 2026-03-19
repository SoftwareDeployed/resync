open Melange_json.Primitives;

type decoder('patch) = StoreJson.json => option('patch);
type updater('config) = 'config => 'config;

let flatMap = (optionValue, mapper) =>
  switch (optionValue) {
  | Some(value) => mapper(value)
  | None => None
  };

let compose = (decoders: list(decoder('patch))): decoder('patch) =>
  json => decoders |> List.find_map(decoder => decoder(json));

module Pg = {
  type event('row) =
    | Insert('row)
    | Update('row)
    | Delete(string);

  let field = (json, key) => {
    let rawJson = (Obj.magic(json): Melange_json.t);
    switch (Melange_json.classify(rawJson)) {
    | `Assoc(entries) =>
      entries
      |> List.find_map(((entryKey, value)) =>
           entryKey == key ? Some(Obj.magic(value)) : None
         )
    | _ => None
    };
  };

  let decode = (~table, ~decodeRow, json): option(event('row)) =>
    switch (
      flatMap(field(json, "type"), StoreJson.tryDecode(string_of_json)),
      flatMap(field(json, "table"), StoreJson.tryDecode(string_of_json)),
      flatMap(field(json, "action"), StoreJson.tryDecode(string_of_json)),
    ) {
    | (Some("patch"), Some(patchTable), Some(action)) when patchTable == table =>
      switch (action) {
      | "INSERT" =>
        flatMap(field(json, "data"), StoreJson.tryDecode(decodeRow))
        |> Option.map(inserted => Insert(inserted))
      | "UPDATE" =>
        flatMap(field(json, "data"), StoreJson.tryDecode(decodeRow))
        |> Option.map(updated => Update(updated))
      | "DELETE" =>
        flatMap(field(json, "id"), StoreJson.tryDecode(string_of_json))
        |> Option.map(deletedId => Delete(deletedId))
      | _ => None
      }
    | _ => None
    };

  let decodeAs = (~table, ~decodeRow, ~insert, ~update, ~delete, ()): decoder('patch) =>
    json =>
      switch (decode(~table, ~decodeRow, json)) {
      | Some(Insert(row)) => Some(insert(row))
      | Some(Update(row)) => Some(update(row))
      | Some(Delete(id)) => Some(delete(id))
      | None => None
      };
};
