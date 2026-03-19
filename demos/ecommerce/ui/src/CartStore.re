open Melange_json.Primitives;

module Schema = {
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
};

module Reservation = Schema.Reservation;
module CartItem = Schema.CartItem;

module Core = StoreCore.Make(Schema);

let setItemsRef = ref((_items: Js.Dict.t(CartItem.t)) => ());
let currentItemsRef = ref(Js.Dict.empty());

let log = (label: string, value: 'a) =>
  switch%platform (Runtime.platform) {
  | Client => Js.log2(label, value)
  | Server => ()
  };

let sourceItems = (config: Schema.config): Schema.config =>
  switch%platform (Runtime.platform) {
  | Client => {
      log("[cart] sourceItems init count", config.items->Js.Dict.keys->Array.length);
      currentItemsRef := config.items;
      {
        items:
          Tilia.Core.source(. config.items, (. _items, setItems) => {
            setItemsRef := nextItems => {
              log("[cart] sourceItems next count", nextItems->Js.Dict.keys->Array.length);
              currentItemsRef := nextItems;
              setItems(nextItems);
            };
          }),
      };
    }
  | Server => config
  };

module Persist = StorePersist.Make({
  type payload = Schema.payload;
  type store = Schema.store;

  let storageKey = Schema.storageKey;
  let emptyStore = Schema.emptyStore;
  let makeStore = payload => Core.buildStore(~configTransform=sourceItems, payload);
  let payloadOfStore = Schema.payloadOfStore;
  let payload_of_json = Schema.payload_of_json;
  let payload_to_json = Schema.payload_to_json;
});

include Core;

let createStore = (config: Schema.config) => {
  let store = Core.createStore(~configTransform=sourceItems, config);
  currentItemsRef := store.items;
  store;
};

let hydrateStore = () => {
  let store = Persist.hydrateStore();
  currentItemsRef := store.items;
  log("[cart] hydrated count", store.items->Js.Dict.keys->Array.length);
  store;
};

let copyItems = (items: Js.Dict.t(CartItem.t)) => {
  let nextItems = Js.Dict.empty();
  items
  ->Js.Dict.keys
  ->Belt.Array.forEach(key =>
      switch (items->Js.Dict.get(key)) {
      | Some(item) => nextItems->Js.Dict.set(key, item)
      | None => ()
      }
    );
  nextItems;
};

let add_to_cart = (_store: t, item: Config.InventoryItem.t) => {
  let currentItems =
    switch%platform (Runtime.platform) {
    | Client => currentItemsRef.contents
    | Server => _store.items
    };
  let beforeCount = currentItems->Js.Dict.keys->Array.length;
  let nextItems = copyItems(currentItems);
  let nextStore: Schema.payload = {items: nextItems};
  Schema.addItem(nextStore, item);
  let afterCount = nextItems->Js.Dict.keys->Array.length;
  log("[cart] add_to_cart item", item.id);
  log("[cart] add_to_cart before count", beforeCount);
  log("[cart] add_to_cart after count", afterCount);
  currentItemsRef := nextItems;
  setItemsRef.contents(nextItems);
  log("[cart] persisting payload", nextItems);
  Persist.persistPayload({items: nextItems});
};
