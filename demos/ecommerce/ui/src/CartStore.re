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
type state = {
  items: StoreJson.Dict.t(CartItem.t),
  updated_at: float,
};

type items = StoreJson.Dict.t(CartItem.t);

type quantity_input = {
  inventory_id: string,
  quantity: int,
};

type action =
  | SetItemQuantity(quantity_input)
  | RemoveItem(string)
  | ClearCart;

type store = {
  state: state,
  item_count: int,
};

let storeName = "ecommerce.cart";
let stateElementId = "cart-store";

let defaultItems: items = Js.Dict.empty();

let normalizeItems = items =>
  try ({
    ignore(items->Js.Dict.keys);
    items;
  }) {
  | _ => defaultItems
  };

let emptyState: state = {
  items: defaultItems,
  updated_at: 0.0,
};

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

let action_to_json = action =>
  switch (action) {
  | SetItemQuantity(input) =>
    StoreJson.parse(
      "{\"kind\":\"set_item_quantity\",\"inventory_id\":"
      ++ string_to_json(input.inventory_id)->Melange_json.to_string
      ++ ",\"quantity\":"
      ++ int_to_json(input.quantity)->Melange_json.to_string
      ++ "}",
    )
  | RemoveItem(id) =>
    StoreJson.parse(
      "{\"kind\":\"remove_item\",\"inventory_id\":"
      ++ string_to_json(id)->Melange_json.to_string
      ++ "}",
    )
  | ClearCart => StoreJson.parse("{\"kind\":\"clear_cart\"}")
  };

let action_of_json = json => {
  let kind =
    StoreJson.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
  switch (kind) {
  | "set_item_quantity" =>
    SetItemQuantity({
      inventory_id:
        StoreJson.requiredField(
          ~json,
          ~fieldName="inventory_id",
          ~decode=string_of_json,
        ),
      quantity:
        StoreJson.requiredField(~json, ~fieldName="quantity", ~decode=int_of_json),
    })
  | "remove_item" =>
    RemoveItem(
      StoreJson.requiredField(
        ~json,
        ~fieldName="inventory_id",
        ~decode=string_of_json,
      ),
    )
  | _ => ClearCart
  };
};

let reduce = (~state: state, ~action: action) => {
  let updated_at = Js.Date.now();
  switch (action) {
  | SetItemQuantity(input) => {
      items:
        setItemQuantity(
          copyItems(state.items),
          ~inventoryId=input.inventory_id,
          ~quantity=input.quantity,
        ),
      updated_at,
    }
  | RemoveItem(inventoryId) => {
      items: removeItemById(state.items, inventoryId),
      updated_at,
    }
  | ClearCart => {
      items: defaultItems,
      updated_at,
    }
  };
};

/* ============================================================================
   Pipeline Builder API
   ============================================================================ */

module StoreDef =
  StoreBuilder.Local.Define({
    type nonrec state = state;
    type nonrec action = action;
    type nonrec store = store;

    let input =
      StoreBuilder.make()
      |> StoreBuilder.withSchema({
           emptyState,
           reduce,
           makeStore:
             (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
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
           },
         })
      |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
      |> StoreBuilder.withLocalPersistence(
           ~storeName,
           ~scopeKeyOfState = (_state: state) => "default",
           ~timestampOfState = (state: state) => state.updated_at,
           ~stateElementId=Some(stateElementId),
           (),
         );
  });

include (
  StoreDef:
    StoreBuilder.Runtime.Exports
      with type state := state
      and type action := action
      and type t := store
);

type t = store;

module Context = StoreDef.Context;

let setQuantity = (store: t, ~inventoryId, ~quantity) => {
  let _ = store;
  dispatch(SetItemQuantity({inventory_id: inventoryId, quantity}));
};

let increment_item = (store: t, inventoryId: string) => {
  let nextQuantity =
    switch (store.state.items->Js.Dict.get(inventoryId)) {
    | Some((cartItem: CartItem.t)) => cartItem.quantity + 1
    | None => 1
    };
  setQuantity(store, ~inventoryId, ~quantity=nextQuantity);
};

let decrement_item = (store: t, inventoryId: string) => {
  let nextQuantity =
    switch (store.state.items->Js.Dict.get(inventoryId)) {
    | Some((cartItem: CartItem.t)) => cartItem.quantity - 1
    | None => 0
    };
  setQuantity(store, ~inventoryId, ~quantity=nextQuantity);
};

let remove_item = (_store: t, inventoryId: string) => dispatch(RemoveItem(inventoryId));

let add_to_cart = (store: t, item: Model.InventoryItem.t) => {
  increment_item(store, item.id);
};

let clear = () => dispatch(ClearCart);
