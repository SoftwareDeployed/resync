module Reservation = {
  type t = {
    // Date of the reservation
    date: Js.Date.t,
    // If the unit_type is hour and units is set to 4, the reservation will end 4 hours after the date.
    // There is probably a better way to model this.
    units: int,
    unit_type: PeriodList.Unit.t,
  };
};

module CartItem = {
  type t = {
    reservation: option(Reservation.t),
    // This is the ID of an InventoryItem.t. There is probably a better way to model this.
    inventory_id: int,
    quantity: int,
  };
};

module CartStore = {
  type t = {items: Js.Dict.t(CartItem.t)};
  // For now make is a stub that returns an empty cart. This will eventually restore the user's previous cart.
  // A feature I would really like to have is real time cart sharing. Use cases include: group ordering, third party cart payment, etc.

  let state =
    switch%platform (Runtime.platform) {
    | Server => { items: Js.Dict.fromArray([||]) }
    | Client => Tilia.Core.make({ items: Js.Dict.fromArray([||]) })
    };
  let add_to_cart = (item: Config.InventoryItem.t) => {
    let cart_item =
      switch (state.items->Js.Dict.get(item.id->Int.to_string)) {
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

    state.items->Js.Dict.set(item.id->Int.to_string, cart_item);
  };
};

type t = {
  // Perhaps the ID should be typed as a UUID?
  premise_id: string,
  config: Config.t,
  period_list: array(Config.Pricing.period),
  unit: PeriodList.Unit.t,
};

let deriveConfig = (store: t) => store.config;

let derivePremiseId = (config: Config.t) => {
  let premise = config.premise->Belt.Option.getUnsafe;
  premise.id;
};

let derivePeriodList = (config: Config.t) => {
  let inventory = config.inventory->Belt.Array.copy;
  let seen = [||];
  let periods: array(Config.Pricing.period) = [||];
  inventory->Belt.Array.forEach(inv => {
    inv.period_list
    ->Belt.Array.forEach((pl: Config.Pricing.period) =>
        if (seen->Belt.Array.some(u => u == pl.unit)) {
          periods->Belt.Array.push(pl) |> ignore;
          seen->Belt.Array.push(pl.unit) |> ignore;
        }
      )
  });
  periods;
};

// Create a plain Store.t record (no Tilia wrapping) - used on server
let createPlainStore = (~config: Config.t, ~unit: PeriodList.Unit.t) => {
  premise_id: config.premise->Belt.Option.map(p => p.id)->Belt.Option.getWithDefault(""),
  config: config,
  period_list: derivePeriodList(config),
  unit: unit,
};

// Default/empty store value
let empty: t = {
  premise_id: "",
  config: Config.SSR.empty,
  period_list: [||],
  unit: PeriodList.Unit.defaultState,
};

// Create a Tilia-wrapped store with reactive derived fields - used on client
let%browser_only makeStore = (~config: Config.t, ~unit: PeriodList.Unit.t) =>
  Tilia.Core.carve(({ derived }) => {
    {
      premise_id: derived(store => store->deriveConfig->derivePremiseId),
      config: config,
      period_list: derived(store => store->deriveConfig->derivePeriodList),
      unit: unit,
    }
  });
