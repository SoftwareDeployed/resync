[@deriving json]
type config = Model.t;

type subscription = RealtimeSubscription.t;

type patch = StoreCrud.patch(Model.InventoryItem.t);

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

let emptyConfig: config = Model.SSR.empty;

let action_to_json = _action => StoreJson.parse("{\"kind\":\"noop\"}");

let action_of_json = _json => Noop;

let reduce = (~state: config, ~action: action) => {
  let _ = action;
  state;
};

let project = (config: config): projections => {
  premise_id:
    switch (config.premise) {
    | None => ""
    | Some(premise) => premise.id
    },
  period_list:
    config.inventory
    |> Array.to_list
    |> List.map((inv: Model.InventoryItem.t) => Array.to_list(inv.period_list))
    |> List.concat
    |> List.fold_left(
         (periods: list(Model.Pricing.period), period: Model.Pricing.period) => {
           let current_unit = period.unit;
           if (
             List.exists(
               (existing: Model.Pricing.period) => existing.unit == current_unit,
               periods,
             )
           ) {
             periods;
           } else {
             [period, ...periods];
           };
         },
         []: list(Model.Pricing.period),
       )
    |> List.rev
    |> Array.of_list,
};

let setTimestamp = (~state: config, ~timestamp: float) =>
  switch (state.premise) {
  | Some(premise) => {
      ...state,
      premise: Some({...premise, updated_at: Js.Date.fromFloat(timestamp)}),
    }
  | None => state
  };

module Runtime = StoreBuilder.Runtime.MakeSynced({
  type nonrec state = config;
  type nonrec action = action;
  type nonrec store = store;
  type nonrec subscription = subscription;
  type nonrec patch = patch;

  let reduce = reduce;
  let emptyState = emptyConfig;
  let storeName = "ecommerce.inventory";
  let stateElementId = "initial-store";
  let scopeKeyOfState = (config: config) =>
    switch (config.premise) {
    | Some(premise) => premise.id
    | None => "default"
    };
  let timestampOfState = (config: config) =>
    switch (config.premise) {
    | Some(premise) => premise.updated_at->Js.Date.getTime
    | None => 0.0
    };
  let setTimestamp = setTimestamp;
  let state_of_json = config_of_json;
  let state_to_json = config_to_json;
  let action_of_json = action_of_json;
  let action_to_json = action_to_json;
  let makeStore = (~state: config, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
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
  let subscriptionOfState = (config: config): option(subscription) =>
    switch (config.premise) {
    | Some(premise) => Some(RealtimeSubscription.premise(premise.id))
    | None => None
    };
  let encodeSubscription = RealtimeSubscription.encode;
  let eventUrl = Constants.event_url;
  let baseUrl = RealtimeClient.Socket.defaultBaseUrl;
  let decodePatch =
    StorePatch.compose([
      StoreCrud.decodePatch(
        ~table=RealtimeSchema.table_name("inventory"),
        ~decodeRow=Model.InventoryItem.of_json,
        (),
      ),
    ]);
  let updateOfPatch =
    StoreCrud.updateOfPatch(
      ~getId=(item: Model.InventoryItem.t) => item.id,
      ~getItems=(config: config) => config.inventory,
      ~setItems=(config: config, items) => {...config, inventory: items},
    );
  let onActionError = _message => ();
});

include (
  Runtime:
    StoreBuilder.Runtime.Exports
      with type state := config
      and type action := action
      and type t := store
);

type t = store;

module Context = Runtime.Context;

module CartStore = CartStore;
