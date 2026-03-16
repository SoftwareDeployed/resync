module Core = StoreCore.Make(CartStoreSchema);

let setItemsRef = ref((_items: Js.Dict.t(CartStoreSchema.CartItem.t)) => ());

let log = (label: string, value: 'a) =>
  switch%platform (Runtime.platform) {
  | Client => Js.log2(label, value)
  | Server => ()
  };

let sourceItems = (config: CartStoreSchema.config): CartStoreSchema.config =>
  switch%platform (Runtime.platform) {
  | Client => {
      log("[cart] sourceItems init count", config.items->Js.Dict.keys->Array.length);
      {
        items:
          Tilia.Core.source(. config.items, (. _items, setItems) => {
            setItemsRef := nextItems => {
              log("[cart] sourceItems next count", nextItems->Js.Dict.keys->Array.length);
              setItems(nextItems);
            };
          }),
      };
    }
  | Server => config
  };

module Persist = StorePersist.Make({
  type payload = CartStoreSchema.payload;
  type store = CartStoreSchema.store;

  let storageKey = CartStoreSchema.storageKey;
  let emptyStore = CartStoreSchema.emptyStore;
  let makeStore = payload => Core.buildStore(~configTransform=sourceItems, payload);
  let payloadOfStore = CartStoreSchema.payloadOfStore;
  let decodePersisted = CartStoreSchema.decodePersisted;
  let encodePersisted = CartStoreSchema.encodePersisted;
});

include Core;

let createStore = (config: CartStoreSchema.config) =>
  Core.createStore(~configTransform=sourceItems, config);

let hydrateStore = () => {
  let store = Persist.hydrateStore();
  log("[cart] hydrated count", store.items->Js.Dict.keys->Array.length);
  store;
};

let copyItems = (items: Js.Dict.t(CartStoreSchema.CartItem.t)) => {
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

let add_to_cart = (store: t, item: Config.InventoryItem.t) => {
  let beforeCount = store.items->Js.Dict.keys->Array.length;
  let nextItems = copyItems(store.items);
  let nextStore: CartStoreSchema.payload = {items: nextItems};
  CartStoreSchema.addItem(nextStore, item);
  let afterCount = nextItems->Js.Dict.keys->Array.length;
  log("[cart] add_to_cart item", item.id);
  log("[cart] add_to_cart before count", beforeCount);
  log("[cart] add_to_cart after count", afterCount);
  setItemsRef.contents(nextItems);
  log("[cart] persisting payload", nextItems);
  Persist.persistPayload({items: nextItems});
};
