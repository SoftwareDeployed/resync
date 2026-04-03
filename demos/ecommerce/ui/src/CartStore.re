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
type state = {items: StoreJson.Dict.t(CartItem.t)};

type items = StoreJson.Dict.t(CartItem.t);

[@deriving json]
type payload = {
  [@json.option]
  items: option(StoreJson.Dict.t(CartItem.t)),
};

type store = {
  state: state,
  item_count: int,
};

let storageKey = "ecommerce.cart";
let stateElementId = "cart-store";

let defaultItems: items = Js.Dict.empty();

let normalizeItems = items =>
  try({
    ignore(items->Js.Dict.keys);
    items;
  }) {
  | _ => defaultItems
  };

let emptyPayload: payload = {
  items: Some(defaultItems),
};

let payloadOfState = (state: state): payload => {items: Some(state.items)};

let stateOfPayload = (payload: payload): state => {
  items:
    switch (payload.items) {
    | Some(items) => normalizeItems(items)
    | None => defaultItems
    },
};

let emptyState = stateOfPayload(emptyPayload);

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

let itemCount = (state: state) => itemCountOfItems(state.items);

let makeStore =
    (
      ~state: state,
      ~payload: payload,
      ~derive: option(Tilia.Core.deriver(store))=?,
      (),
    ):
    store => {
  let _ = payload;
  {
    state:
      StoreBuilder.current(
        ~derive?,
        ~client=state,
        ~server=() => state,
        (),
      ),
    item_count:
      StoreBuilder.derived(
        ~derive?,
        ~client=store => itemCount(store.state),
        ~server=() => itemCount(state),
        (),
      ),
  };
};

let emptyStore = makeStore(~state=emptyState, ~payload=emptyPayload, ());

let log = (label: string, value: 'a) =>
  switch%platform (Runtime.platform) {
  | Client => Js.log2(label, value)
  | Server => ()
  };

module Local = StoreLocal.Make({
  type nonrec state = state;
  type nonrec payload = payload;

  module Adapter = StoreLocal.LocalStorageAdapter;

  let storageKeyOfState = (_state: state) => storageKey;
  let payloadOfState = payloadOfState;
  let stateOfPayload = stateOfPayload;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
});

module Layered = StoreBuilder.Layered.Make({
  type nonrec state = state;
  type nonrec payload = payload;
  type nonrec store = store;

  let emptyStore = emptyStore;
  let emptyPayload = emptyPayload;
  let stateElementId = stateElementId;
  let payloadOfState = payloadOfState;
  let stateOfPayload = stateOfPayload;
  let state_of_json = state_of_json;
  let state_to_json = state_to_json;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
  let clientLayers = [|Local.hooks|];
  let makeStore = makeStore;
});

include (
  Layered:
    StoreBuilder.Layered.Exports
      with type state := state
      and type payload := payload
      and type t := store
);

type t = store;

module Context = Layered.Context;

let hydrateStore = () => {
  let store = Layered.hydrateStore();
  log("[cart] hydrated", "ok");
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

let setItemQuantity = (items: items, ~inventoryId, ~quantity) =>
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

let updateItems = (~store: t, ~label, reducer) => {
  let beforeCount = itemCount(store.state);
  log(label ++ " before count", beforeCount);
  Local.update(state => {
    let nextItems = reducer(copyItems(state.items));
    log(label ++ " persisting", nextItems);
    {items: nextItems};
  });
  switch (Local.get()) {
  | Some(state) => log(label ++ " after count", itemCount(state))
  | None => ()
  };
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

let add_to_cart = (store: t, item: Model.InventoryItem.t) => {
  log("[cart] add_to_cart item", item.id);
  increment_item(store, item.id);
};

let clear = () => Local.clearCurrent();
