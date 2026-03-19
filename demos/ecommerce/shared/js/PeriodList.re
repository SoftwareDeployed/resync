open Melange_json.Primitives;

module Unit = {
  [@deriving jsConverter]
  type t = [
    | [@mel.as "second"] `Second
    | [@mel.as "minute"] `Minute
    | [@mel.as "hour"] `Hour
    | [@mel.as "day"] `Day
    | [@mel.as "week"] `Week
    | [@mel.as "month"] `Month
    | [@mel.as "year"] `Year
  ];

  let of_json = json =>
    switch (json->string_of_json->tFromJs) {
    | Some(unit) => unit
    | None => Melange_json.of_json_error(~json, "expected supported period unit")
    };

  let to_json = unit => unit->tToJs->string_to_json;

  // XXX: This default state should come from the server
  let defaultState: t = `Month;
  let state = StoreSignal.make(defaultState);
  let value = state.value;
  let get = state.get;
  let set = state.set;
};

module Premise = {
  [@deriving json]
  type t = {
    id: string,
    name: string,
    description: string,
    updated_at: StoreJson.Date.t,
  };
};
