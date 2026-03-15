type json;

[@platform native]
let parseJson = _value => Obj.magic(());
[@platform js]
[@mel.scope "JSON"] external parseJson: string => json = "parse";

[@platform native]
let stringifyJson = _value => "";
[@platform js]
[@mel.scope "JSON"] external stringifyJson: 'a => string = "stringify";

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

type config = {items: Js.Dict.t(CartItem.t)};
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

let reservationFromJson = (json: Js.Json.t): Reservation.t => {
  let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
  {
    date: Obj.magic(Js.Dict.unsafeGet(dict, "date"))->Js.Date.fromString,
    units: (Obj.magic(Js.Dict.unsafeGet(dict, "units")): int),
    unit_type:
      switch (PeriodList.Unit.tFromJs(Obj.magic(Js.Dict.unsafeGet(dict, "unit_type")))) {
      | Some(unit) => unit
      | None => PeriodList.Unit.defaultState
      },
  };
};

let cartItemFromJson = (json: Js.Json.t): CartItem.t => {
  let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
  let reservation =
    switch (Js.Dict.get(dict, "reservation")) {
    | Some(value) =>
      switch%platform (Runtime.platform) {
      | Client =>
        if (value == Js.Json.null) {
          None;
        } else {
          Some(reservationFromJson(value));
        }
      | Server => None
      }
    | None => None
    };
  {
    reservation,
    inventory_id: (Obj.magic(Js.Dict.unsafeGet(dict, "inventory_id")): string),
    quantity: (Obj.magic(Js.Dict.unsafeGet(dict, "quantity")): int),
  };
};

let decodePersisted = (value: string): payload =>
  switch%platform (Runtime.platform) {
  | Client =>
    try({
      let dict: Js.Dict.t(Js.Json.t) = Obj.magic(parseJson(value));
      let itemsJson =
        switch (Js.Dict.get(dict, "items")) {
        | Some(json) => (Obj.magic(json): Js.Dict.t(Js.Json.t))
        | None => Js.Dict.empty()
        };
      let items = Js.Dict.empty();
      itemsJson
      ->Js.Dict.keys
      ->Belt.Array.forEach(key =>
          switch (itemsJson->Js.Dict.get(key)) {
          | Some(itemJson) => items->Js.Dict.set(key, cartItemFromJson(itemJson))
          | None => ()
          }
        );
      {items: items};
    }) {
    | _ => emptyPayload
    }
  | Server => emptyPayload
  };

let encodePersisted = (payload: payload) =>
  switch%platform (Runtime.platform) {
  | Client => stringifyJson(Obj.magic(payload))
  | Server => ""
  };

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
