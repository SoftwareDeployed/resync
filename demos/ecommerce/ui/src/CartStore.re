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

type items = StoreJson.Dict.t(CartItem.t);

[@deriving json]
type payload = {
  [@json.option]
  items: option(StoreJson.Dict.t(CartItem.t)),
};

type store = {
  items,
  item_count: int,
};

let storageKey = "ecommerce.cart";

let defaultItems: items = Js.Dict.empty();

let emptyPayload: payload = {
  items: Some(defaultItems),
};

let payloadOfConfig = (config: config): payload => {items: Some(config.items)};

let configOfPayload = (payload: payload): config => {
  items:
    switch (payload.items) {
    | Some(items) => items
    | None => defaultItems
    },
};

let payloadOfStore = (store: store): payload => {items: Some(store.items)};

let emptyConfig = configOfPayload(emptyPayload);

let itemCountOfItems = items =>
  Array.fold_left(
    (count, inventoryId) =>
      count
      + switch (items->Js.Dict.get(inventoryId)) {
        | Some((cartItem: CartItem.t)) => cartItem.quantity
        | None => 0
        },
    0,
    items->Js.Dict.keys,
  );

let itemCount = (config: config) => itemCountOfItems(config.items);

let makeStore =
    (
      ~config: config,
      ~payload: payload,
      ~derive: option(Tilia.Core.deriver(store))=?,
      (),
    ):
    store => {
  let _ = payload;
  {
    items: config.items,
    item_count:
      StoreBuilder.derived(
        ~derive?,
        ~client=store => itemCountOfItems(store.items),
        ~server=() => itemCount(config),
        (),
      ),
  };
};

let emptyStore = makeStore(~config=emptyConfig, ~payload=emptyPayload, ());

let itemsSourceRef: ref(option(StoreSource.t(items))) = ref(None);

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
  let makeStore = makeStore;
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

let copyItems = (items: items) => {
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

let removeItemById = (items: items, inventoryId: string) => {
  let nextItems = Js.Dict.empty();
  items
  ->Js.Dict.keys
  ->Belt.Array.forEach(key =>
      if (key != inventoryId) {
        switch (items->Js.Dict.get(key)) {
        | Some(item) => nextItems->Js.Dict.set(key, item)
        | None => ()
        };
      }
    );
  nextItems;
};

let setItemQuantity = (items: items, ~inventoryId, ~quantity) => {
  if (quantity <= 0) {
    removeItemById(items, inventoryId);
  } else {
    let nextItems = copyItems(items);
    let nextItem =
      switch (nextItems->Js.Dict.get(inventoryId)) {
      | Some(existing) => {
          ...existing,
          quantity,
        }
      | None => {
          reservation: None,
          inventory_id: inventoryId,
          quantity,
        }
      };
    nextItems->Js.Dict.set(inventoryId, nextItem);
    nextItems;
  };
};

let updateItems = (~store: t, ~label, reducer) => {
  let source = itemsSourceRef.contents;
  let currentItems =
    switch (source) {
    | Some(itemsSource) => itemsSource.get()
    | None => store.items
    };
  let beforeCount = itemCountOfItems(currentItems);
  let nextItems =
    switch (source) {
    | Some(itemsSource) => {
        itemsSource.update(current => reducer(copyItems(current)));
        itemsSource.get();
      }
    | None => reducer(copyItems(currentItems))
    };
  let afterCount = itemCountOfItems(nextItems);
  log(label ++ " before count", beforeCount);
  log(label ++ " after count", afterCount);
  log(label ++ " persisting", nextItems);
  Runtime.persistPayload({items: Some(nextItems)});
};

let increment_item = (store: t, inventoryId: string) => {
  log("[cart] increment item", inventoryId);
  updateItems(~store, ~label="[cart] increment", items => {
    let nextQuantity =
      switch (items->Js.Dict.get(inventoryId)) {
      | Some((cartItem: CartItem.t)) => cartItem.quantity + 1
      | None => 1
      };
    setItemQuantity(items, ~inventoryId, ~quantity=nextQuantity);
  });
};

let decrement_item = (store: t, inventoryId: string) => {
  log("[cart] decrement item", inventoryId);
  updateItems(~store, ~label="[cart] decrement", items => {
    let nextQuantity =
      switch (items->Js.Dict.get(inventoryId)) {
      | Some((cartItem: CartItem.t)) => cartItem.quantity - 1
      | None => 0
      };
    setItemQuantity(items, ~inventoryId, ~quantity=nextQuantity);
  });
};

let remove_item = (store: t, inventoryId: string) => {
  log("[cart] remove item", inventoryId);
  updateItems(~store, ~label="[cart] remove", items =>
    removeItemById(items, inventoryId)
  );
};

let add_to_cart = (store: t, item: Config.InventoryItem.t) => {
  log("[cart] add_to_cart item", item.id);
  increment_item(store, item.id);
};
