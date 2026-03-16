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
  // XXX: This default state should come from the server
  let defaultState: t = `Month;
  let set = (_unit: t) => ();
  //let (signal, set) = Tilia.signal(defaultState);
};

module Premise = {
  type t = {
    id: string,
    name: string,
    description: string,
    updated_at: Js.Date.t,
  };
};
