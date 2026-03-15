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

module CartStore = {
  type t = {items: Js.Dict.t(CartItem.t)};

  let state =
    switch%platform (Runtime.platform) {
    | Server => { items: Js.Dict.fromArray([||]) }
    | Client => Tilia.Core.make({ items: Js.Dict.fromArray([||]) })
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
};

type t = {
  premise_id: string,
  config: Config.t,
  period_list: array(Config.Pricing.period),
  unit: PeriodList.Unit.t,
};

let derivePremiseId = (config: Config.t) => {
  switch (config.premise) {
  | None => ""
  | Some(premise) => premise.id
  };
};

let derivePeriodList = (config: Config.t) => {
  let seen = ref([]);
  let periods = ref([]);
  config.inventory->Belt.Array.forEach(inv => {
    inv.period_list
    ->Belt.Array.forEach((pl: Config.Pricing.period) =>
        if (!(seen.contents |> List.exists(u => u == pl.unit))) {
          seen := [pl.unit, ...seen.contents];
          periods := [pl, ...periods.contents];
        }
      )
  });
  periods.contents |> List.rev |> Array.of_list;
};

let createStore = (~config: Config.t, ~unit: PeriodList.Unit.t) => {
  premise_id: derivePremiseId(config),
  config,
  period_list: derivePeriodList(config),
  unit,
};

let empty: t = {
  premise_id: "",
  config: Config.SSR.empty,
  period_list: [||],
  unit: PeriodList.Unit.defaultState,
};

let%browser_only makeStore = (~config: Config.t) => {
  Js.log2("making store with config:", config);
  let currentConfig = ref(config);
  Tilia.Core.make({
    config:
      Tilia.Core.source(. config, (. _config, set) => {
        let setConfig = nextConfig => {
          currentConfig := nextConfig;
          set(nextConfig);
        };

        switch (config.premise) {
        | None => ()
        | Some(premise) =>
          let premiseId = premise.id;
          let updatedAt = premise.updated_at->Js.Date.getTime;
          let getConfig = () => currentConfig.contents;
          Client.Socket.subscribe(setConfig, getConfig, premiseId, updatedAt);
        }
      }),
    premise_id:
      switch (config.premise) {
      | Some(p) => p.id
      | None => ""
      },
    period_list: derivePeriodList(config),
    unit: PeriodList.Unit.defaultState,
  });
};
