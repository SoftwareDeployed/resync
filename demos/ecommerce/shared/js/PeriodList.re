open Melange_json.Primitives;

module Unit = {
  [@deriving (jsConverter, json)]
  type t = [
    | [@json.name "second"] [@mel.as "second"] `Second
    | [@json.name "minute"] [@mel.as "minute"] `Minute
    | [@json.name "hour"] [@mel.as "hour"] `Hour
    | [@json.name "day"] [@mel.as "day"] `Day
    | [@json.name "week"] [@mel.as "week"] `Week
    | [@json.name "month"] [@mel.as "month"] `Month
    | [@json.name "year"] [@mel.as "year"] `Year
  ];

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
