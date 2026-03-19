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

let itemCount = (config: config) => config.items->Js.Dict.keys->Array.length;
let itemCountOfItems = items => items->Js.Dict.keys->Array.length;

let makeClientStore = (~config: config, ~payload as _, ~derive: Tilia.Core.deriver(store)): store => {
  items: config.items,
  item_count: derive.derived(store => itemCountOfItems(store.items)),
};

let makeServerStore = (~config: config, ~payload as _): store => {
  items: config.items,
  item_count: itemCount(config),
};

let emptyStore = makeServerStore(~config=emptyPayload, ~payload=emptyPayload);

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

let itemsSourceRef: ref(option(StoreSource.t(Js.Dict.t(CartItem.t)))) = ref(None);

let log = (label: string, value: 'a) =>
  switch%platform (Runtime.platform) {
  | Client => Js.log2(label, value)
  | Server => ()
  };

let transformConfig = (config: config): config =>
  switch%platform (Runtime.platform) {
  | Client => {
      log("[cart] sourceItems init count", config.items->Js.Dict.keys->Array.length);
      let itemsSource = StoreSource.make(config.items);
      itemsSourceRef := Some(itemsSource);
      {items: itemsSource.value};
    }
  | Server => {
      let itemsSource = StoreSource.make(config.items);
      itemsSourceRef := Some(itemsSource);
      {items: itemsSource.value};
    }
  };

module Runtime = StoreBuilder.Persisted.Make({
  type nonrec config = config;
  type nonrec payload = payload;
  type nonrec store = store;

  let storageKey = storageKey;
  let emptyStore = emptyStore;
  let payloadOfConfig = payloadOfConfig;
  let configOfPayload = configOfPayload;
  let payloadOfStore = payloadOfStore;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
  let transformConfig = transformConfig;
  let makeClientStore = makeClientStore;
  let makeServerStore = makeServerStore;
});

include (
  Runtime:
    StoreBuilder.Persisted.Exports
      with type config := config
      and type payload := payload
      and type t := store
);

type t = store;

module Context = Runtime.Context;

let hydrateStore = () => {
  let store = Runtime.hydrateStore();
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
  let source = itemsSourceRef.contents;
  let currentItems =
    switch (source) {
    | Some(itemsSource) => itemsSource.get()
    | None => _store.items
    };
  let beforeCount = currentItems->Js.Dict.keys->Array.length;
  let nextItems =
    switch (source) {
    | Some(itemsSource) => {
        itemsSource.update(current => {
          let next = copyItems(current);
          let nextStore: payload = {items: next};
          addItem(nextStore, item);
          next;
        });
        itemsSource.get();
      }
    | None => {
        let next = copyItems(currentItems);
        let nextStore: payload = {items: next};
        addItem(nextStore, item);
        next;
      }
    };
  let afterCount = nextItems->Js.Dict.keys->Array.length;
  log("[cart] add_to_cart item", item.id);
  log("[cart] add_to_cart before count", beforeCount);
  log("[cart] add_to_cart after count", afterCount);
  log("[cart] persisting payload", nextItems);
  Runtime.persistPayload({items: nextItems});
};
