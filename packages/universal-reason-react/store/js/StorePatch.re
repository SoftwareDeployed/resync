open Melange_json.Primitives;

type decoder('patch) = StoreJson.json => option('patch);
type updater('config) = 'config => 'config;

let flatMap = (optionValue, mapper) =>
  switch (optionValue) {
  | Some(value) => mapper(value)
  | None => None
  };

let compose = (decoders: array(decoder('patch))): decoder('patch) =>
  json => {
    let rec loop = index =>
      if (index >= Array.length(decoders)) {
        None;
      } else {
        switch (decoders[index](json)) {
        | Some(_) as decoded => decoded
        | None => loop(index + 1)
        };
      };

    loop(0);
  };

module Pg = {
  type event('row) =
    | Insert('row)
    | Update('row)
    | Delete(string);

  let decode = (~table, ~decodeRow, json): option(event('row)) =>
    switch (
      flatMap(StoreJson.field(json, "type"), StoreJson.tryDecode(string_of_json)),
      flatMap(StoreJson.field(json, "table"), StoreJson.tryDecode(string_of_json)),
      flatMap(StoreJson.field(json, "action"), StoreJson.tryDecode(string_of_json)),
    ) {
    | (Some("patch"), Some(patchTable), Some(action)) when patchTable == table =>
      switch (action) {
      | "INSERT" =>
        flatMap(StoreJson.field(json, "data"), StoreJson.tryDecode(decodeRow))
        |> Option.map(inserted => Insert(inserted))
      | "UPDATE" =>
        flatMap(StoreJson.field(json, "data"), StoreJson.tryDecode(decodeRow))
        |> Option.map(updated => Update(updated))
      | "DELETE" =>
        flatMap(StoreJson.field(json, "id"), StoreJson.tryDecode(string_of_json))
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
