module Reservation = {
  type t = {
    date: Js.Date.t,
    units: int,
    unit_type: PeriodList.Unit.t,
  };
};

module CartItem = {
  type t = {
    reservation: option(Reservation.t),
    inventory_id: string,
    quantity: int,
  };
};

type t = {items: Js.Dict.t(CartItem.t)};

let state =
  switch%platform (Runtime.platform) {
  | Server => {items: Js.Dict.fromArray([||])}
  | Client => Tilia.Core.make({items: Js.Dict.fromArray([||])})
  };

let add_to_cart = (item: Config.InventoryItem.t) => {
  let cart_item =
    switch (state.items->Js.Dict.get(item.id)) {
    | Some(item) => {
        ...item,
        quantity: item.quantity + 1,
      }
    | None => {
        reservation: None,
        inventory_id: item.id,
        quantity: 1,
      }
    };

  state.items->Js.Dict.set(item.id, cart_item);
};
