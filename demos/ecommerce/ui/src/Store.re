[@deriving json]
type config = Model.t;

type subscription = RealtimeSubscription.t;

type patch = StoreCrud.patch(Model.InventoryItem.t);

[@deriving json]
type payload = {
  config: config,
  unit: PeriodList.Unit.t,
};

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

module Runtime = StoreBuilder.Runtime.Make({
  type nonrec config = config;
  type nonrec patch = patch;
  type nonrec payload = payload;
  type nonrec store = store;
  type nonrec subscription = subscription;

  let emptyStore: store = {
    premise_id: "",
    config: Model.SSR.empty,
    period_list: [||],
    unit: PeriodList.Unit.defaultState,
  };
  let stateElementId = "initial-store";

  let payloadOfConfig = (config: config): payload => {
    config,
    unit:
      switch%platform (Runtime.platform) {
      | Server => PeriodList.Unit.defaultState
      | Client => PeriodList.Unit.get()
      },
  };
  let configOfPayload = (payload: payload): config => payload.config;

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
                 (existing: Model.Pricing.period) => {
                   let existing_unit = existing.unit;
                   existing_unit == current_unit;
                 },
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

  let makeStore =
      (
        ~config: config,
        ~payload: payload,
        ~derive: option(Tilia.Core.deriver(store))=?,
        (),
      ):
      store => {
    {
      premise_id:
        StoreBuilder.projected(
          ~derive?,
          ~project,
          ~serverSource=config,
          ~fromStore=store => store.config,
          ~select=projections => projections.premise_id,
          (),
        ),
      config,
      period_list:
        StoreBuilder.projected(
          ~derive?,
          ~project,
          ~serverSource=config,
          ~fromStore=store => store.config,
          ~select=projections => projections.period_list,
          (),
        ),
      unit:
        StoreBuilder.current(
          ~derive?,
          ~client=PeriodList.Unit.value,
          ~server=() => payload.unit,
          (),
        ),
    };
  };

  let config_of_json = config_of_json;
  let config_to_json = config_to_json;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;

  let decodePatch =
    StorePatch.compose([
      StoreCrud.decodePatch(
        ~table=RealtimeSchema.table_name("inventory"),
        ~decodeRow=Model.InventoryItem.of_json,
        (),
      ),
    ]);

  let updateOfPatch = StoreCrud.updateOfPatch(
    ~getId=(item: Model.InventoryItem.t) => item.id,
    ~getItems=(config: config) => config.inventory,
    ~setItems=(config: config, items) => {...config, inventory: items},
  );

  let subscriptionOfConfig = (config: config): option(subscription) =>
    switch (config.premise) {
    | Some(premise) => Some(RealtimeSubscription.premise(premise.id))
    | None => None
    };
  let encodeSubscription = RealtimeSubscription.encode;

  let updatedAtOf = (config: config) =>
    switch (config.premise) {
    | Some(premise) => premise.updated_at->Js.Date.getTime
    | None => 0.0
    };
  let eventUrl = Constants.event_url;
  let baseUrl = RealtimeClient.Socket.defaultBaseUrl;
});

include (
  Runtime:
    StoreBuilder.Runtime.Exports
      with type config := config
      and type payload := payload
      and type t := store
);

type t = store;

module Context = Runtime.Context;

module CartStore = CartStore;
