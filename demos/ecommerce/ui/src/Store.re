[@deriving json]
type config = Model.t;

type subscription = RealtimeSubscription.t;

type action = Noop;

type projections = {
  premise_id: string,
  period_list: array(Model.Pricing.period),
};

type store = {
  premise_id: string,
  config: Model.t,
  period_list: array(Model.Pricing.period),
  unit: PeriodList.Unit.t,
};

let action_to_json = _action =>
  StoreJson.Object.make(dict => StoreJson.Object.setString(dict, "kind", "noop"));

let action_of_json = _json => Noop;

let hasPeriodUnit = (periods: array(Model.Pricing.period), unit) =>
  periods->Js.Array.some(~f=(period: Model.Pricing.period) => period.unit == unit);

let appendUniquePeriod = (periods, period: Model.Pricing.period) =>
  if (hasPeriodUnit(periods, period.unit)) {
    periods;
  } else {
    periods->Js.Array.concat(~other=[|period|]);
  };

let appendUniquePeriods = (periods, periodList) =>
  periodList->Js.Array.reduce(~f=appendUniquePeriod, ~init=periods);

let project = (config: config): projections => {
  premise_id:
    switch (config.premise) {
    | None => ""
    | Some(premise) => premise.id
    },
  period_list:
    {
      let rec loop = (index, periods) =>
        if (index >= Array.length(config.inventory)) {
          periods;
        } else {
          loop(index + 1, appendUniquePeriods(periods, config.inventory[index].period_list));
        };

      loop(0, [||]);
    },
};

/* ============================================================================
   FRP CRUD store API
   ============================================================================ */

module StoreDef = StoreFrp.Crud.Build({
  type state = config;
  type nonrec action = action;
  type nonrec store = store;
  type nonrec subscription = subscription;
  type row = Model.InventoryItem.t;

  let setConfigTimestamp = (~state: config, ~timestamp: float) =>
    switch (state.premise) {
    | Some(premise) => {
        ...state,
        premise: Some({...premise, updated_at: Js.Date.fromFloat(timestamp)}),
      }
    | None => state
    };

  let config =
    StoreFrp.Crud.make(
      ~transport={
        subscriptionOfState:
          (config: config): option(subscription) =>
            switch (config.premise) {
            | Some(premise) => Some(RealtimeSubscription.premise(premise.id))
            | None => None
            },
        encodeSubscription: RealtimeSubscription.encode,
        eventUrl: Constants.event_url,
        baseUrl: Constants.base_url,
      },
      ~strategy=
        StoreBuilder.Sync.crud(
          ~table=RealtimeSchema.table_name("inventory"),
          ~decodeRow=Model.InventoryItem.of_json,
          ~getId=(item: Model.InventoryItem.t) => item.id,
          ~getItems=(config: config) => config.inventory,
          ~setItems=(config: config, items) => {...config, inventory: items},
        ),
      {
        storeName: "ecommerce.inventory",
        emptyState: Model.SSR.empty,
        reduce: (~state: config, ~action: action) => {
          let _ = action;
          state;
        },
        state_of_json: config_of_json,
        state_to_json: config_to_json,
        action_of_json,
        action_to_json,
        makeStore:
          (~state: config, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
          {
            premise_id:
              StoreBuilder.projected(
                ~derive?,
                ~project,
                ~serverSource=state,
                ~fromStore=store => store.config,
                ~select=projections => projections.premise_id,
                (),
              ),
            config: state,
            period_list:
              StoreBuilder.projected(
                ~derive?,
                ~project,
                ~serverSource=state,
                ~fromStore=store => store.config,
                ~select=projections => projections.period_list,
                (),
              ),
            unit:
              StoreBuilder.current(
                ~derive?,
                ~client=PeriodList.Unit.value,
                ~server=() => PeriodList.Unit.defaultState,
                (),
              ),
          };
        },
        scopeKeyOfState:
          (config: config) =>
            switch (config.premise) {
            | Some(premise) => premise.id
            | None => "default"
            },
        timestampOfState:
          (config: config) =>
            switch (config.premise) {
            | Some(premise) => premise.updated_at->Js.Date.getTime
            | None => 0.0
            },
        setTimestamp: setConfigTimestamp,
        stateElementId: Some("initial-store"),
      },
    );
});

/* Re-export with the same interface as before */
include (
  StoreDef:
    StoreBuilder.Runtime.Exports
      with type state := config
      and type action := action
      and type t := store
);

type t = store;

module Context = StoreDef.Context;

module CartStore = CartStore;
