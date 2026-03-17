open Melange_json.Primitives;

module Reservation = {
  [@deriving json]
  type t = {
    date: StoreJson.Date.t,
    units: int,
    unit_type: PeriodList.Unit.t,
  };
};

module CartItem = {
  [@deriving json]
  type t = {
    [@json.option] reservation: option(Reservation.t),
    inventory_id: string,
    quantity: int,
  };
};

[@deriving json]
type config = {items: StoreJson.Dict.t(CartItem.t)};

[@deriving json]
type payload = config;

type projections = {
  item_count: int,
};

type store = {
  items: Js.Dict.t(CartItem.t),
  item_count: int,
};

let storageKey = "ecommerce.cart";

let emptyPayload: payload = {
  items: Js.Dict.fromArray([||]),
};

let payloadOfConfig = (config: config): payload => config;
let configOfPayload = (payload: payload): config => payload;
let payloadOfStore = (store: store): payload => {items: store.items};

let project = (config: config): projections => {
  item_count: config.items->Js.Dict.keys->Array.length,
};

let makeStore = (~config: config, ~payload: payload): store =>
  switch%platform (Runtime.platform) {
  | Client =>
    Tilia.Core.carve(derive => {
      let _ = payload;
      {
        items: config.items,
        item_count: derive.derived(store => project({items: store.items}).item_count),
      };
    })
  | Server => {
      let _ = payload;
      let projections = project(config);
      {
        items: config.items,
        item_count: projections.item_count,
      };
    }
  };

let emptyStore =
  makeStore(
    ~config=emptyPayload,
    ~payload=emptyPayload,
  );

let addItem = (config: config, item: Config.InventoryItem.t) => {
  let cartItem =
    switch (config.items->Js.Dict.get(item.id)) {
    | Some(existing) => {
        ...existing,
        quantity: existing.quantity + 1,
      }
    | None => {
        reservation: None,
        inventory_id: item.id,
        quantity: 1,
      }
    };
  config.items->Js.Dict.set(item.id, cartItem);
};
