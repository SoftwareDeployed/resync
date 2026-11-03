open Melange_json.Primitives;

/* The reservation window attached to a cart line when it was added. */
module Reservation = {
  [@deriving json]
  type t = {
    start_date: string,
    end_date: string,
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
  reservation: option(Reservation.t),
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

let itemCountOfItems = items =>
  items
  ->Js.Dict.keys
  ->Js.Array.reduce(
      ~init=0,
      ~f=(count, inventoryId) =>
        count
        + switch (items->Js.Dict.get(inventoryId)) {
          | Some((cartItem: CartItem.t)) => cartItem.quantity
          | None => 0
          },
    );

let itemCount = (state: state) => itemCountOfItems(state.items);

let copyItems = (items: items) => {
  let nextItems = Js.Dict.empty();
  items
  ->Js.Dict.keys
  ->Js.Array.forEach(~f=key =>
      switch (items->Js.Dict.get(key)) {
      | Some(item) => nextItems->Js.Dict.set(key, item)
      | None => ()
      }
    );
  nextItems;
};

let setItemQuantity = (items: items, ~inventoryId, ~quantity, ~reservation) => {
  let nextItems = copyItems(items);
  let existing = nextItems->Js.Dict.get(inventoryId);
  let keptReservation =
    switch (reservation, existing) {
    | (Some(reservation), _) => Some(reservation)
    | (None, Some((prior: CartItem.t))) => prior.reservation
    | (None, None) => None
    };
  nextItems->Js.Dict.set(
    inventoryId,
    {
      reservation: keptReservation,
      inventory_id: inventoryId,
      quantity: max(quantity, 0),
    },
  );
  nextItems;
};

let removeItemById = (items: items, inventoryId: string) => {
  let nextItems = Js.Dict.empty();
  items
  ->Js.Dict.keys
  ->Js.Array.forEach(~f=key =>
      if (key != inventoryId) {
        switch (items->Js.Dict.get(key)) {
        | Some(item) => nextItems->Js.Dict.set(key, item)
        | None => ()
        };
      }
    );
  nextItems;
};

let actionJson = (~kind, ~fill) => {
  StoreJson.Object.make(dict => {
    StoreJson.Object.setString(dict, "kind", kind);
    fill(dict);
  });
};

let action_to_json = action =>
  switch (action) {
  | SetItemQuantity(input) =>
    actionJson(
      ~kind="set_item_quantity",
      ~fill=dict => {
        StoreJson.Object.setString(dict, "inventory_id", input.inventory_id);
        StoreJson.Object.setInt(dict, "quantity", input.quantity);
        switch (input.reservation) {
        | Some(reservation) =>
          StoreJson.Object.setString(dict, "start_date", reservation.start_date);
          StoreJson.Object.setString(dict, "end_date", reservation.end_date);
        | None => ()
        }
      },
    )
  | RemoveItem(id) =>
    actionJson(
      ~kind="remove_item",
      ~fill=dict => StoreJson.Object.setString(dict, "inventory_id", id),
    )
  | ClearCart => actionJson(~kind="clear_cart", ~fill=_dict => ())
  };

let reservation_of_json = json => {
  let start_date =
    StoreJson.optionalField(~json, ~fieldName="start_date", ~decode=string_of_json);
  let end_date =
    StoreJson.optionalField(~json, ~fieldName="end_date", ~decode=string_of_json);
  switch (start_date, end_date) {
  | (Some(start_date), Some(end_date)) =>
    Some(({Reservation.start_date: start_date, end_date: end_date}: Reservation.t))
  | _ => None
  };
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
      reservation: reservation_of_json(json),
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

/* ============================================================================
   Local store (pipeline API)
   ============================================================================ */

module StoreDef =
  (val StoreBuilder.buildLocal(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState: {
           items: defaultItems,
           updated_at: 0.0,
         },
         reduce: (~state: state, ~action: action) => {
           let updated_at = Js.Date.now();
           switch (action) {
           | SetItemQuantity(input) =>
             if (input.quantity <= 0) {
               {
                 items: removeItemById(state.items, input.inventory_id),
                 updated_at,
               };
             } else {
               {
                 items:
                   setItemQuantity(
                     state.items,
                     ~inventoryId=input.inventory_id,
                     ~quantity=input.quantity,
                     ~reservation=input.reservation,
                   ),
                 updated_at,
               };
             }
           | RemoveItem(inventoryId) => {
               items: removeItemById(state.items, inventoryId),
               updated_at,
             }
           | ClearCart => {items: defaultItems, updated_at}
           };
         },
         makeStore:
           (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
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
         },
       })
    |> StoreBuilder.withJson(
         ~state_of_json,
         ~state_to_json,
         ~action_of_json,
         ~action_to_json,
       )
    |> StoreBuilder.withLocalPersistence(
         ~storeName,
         ~scopeKeyOfState=(_state: state) => "default",
         ~timestampOfState=(state: state) => state.updated_at,
         ~stateElementId=Some(stateElementId),
         (),
       )
  ));

include (
  StoreDef:
    StoreBuilder.Runtime.Exports
      with type state := state
      and type action := action
      and type t := store
);

type t = store;

module Context = StoreDef.Context;

let setQuantity = (store: t, ~inventoryId, ~quantity, ~reservation) => {
  let _ = store;
  dispatch(SetItemQuantity({inventory_id: inventoryId, quantity, reservation}));
};

let increment_item = (
  store: t,
  inventoryId: string,
  ~maxQuantity=?,
  ~reservation: option(Reservation.t),
  (),
) => {
  let currentQuantity =
    switch (store.state.items->Js.Dict.get(inventoryId)) {
    | Some((cartItem: CartItem.t)) => cartItem.quantity
    | None => 0
    };
  let nextQuantity =
    switch (maxQuantity) {
    | Some(max) => min(max, currentQuantity + 1)
    | None => currentQuantity + 1
    };
  setQuantity(store, ~inventoryId, ~quantity=nextQuantity, ~reservation);
};

let decrement_item = (store: t, inventoryId: string) => {
  let nextQuantity =
    switch (store.state.items->Js.Dict.get(inventoryId)) {
    | Some((cartItem: CartItem.t)) => cartItem.quantity - 1
    | None => 0
    };
  setQuantity(store, ~inventoryId, ~quantity=nextQuantity, ~reservation=None);
};

let remove_item = (_store: t, inventoryId: string) =>
  dispatch(RemoveItem(inventoryId));

let add_to_cart = (
  store: t,
  item: Model.InventoryItem.t,
  ~maxQuantity=?,
  ~reservation: option(Reservation.t),
  (),
) => increment_item(store, item.id, ~maxQuantity?, ~reservation, ());

let clear = () => dispatch(ClearCart);

/* Snapshot the ?start=&end= selection as the reservation window of a new
   cart line at the moment it is added. */
let reservationFromSearchParams = searchParams => {
  let start =
    switch (UniversalRouter.SearchParams.get("start", searchParams)) {
    | Some(value) => value
    | None => ""
    };
  let end_date =
    switch (UniversalRouter.SearchParams.get("end", searchParams)) {
    | Some(value) => value
    | None => ""
    };
  start != "" && end_date != ""
    ? Some(({Reservation.start_date: start, end_date: end_date}: Reservation.t))
    : None;
};
