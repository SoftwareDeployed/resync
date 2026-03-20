open Melange_json.Primitives;

[@deriving json]
type config = Model.t;

type subscription = RealtimeSubscription.t;

[@deriving json]
type inventory_patch_data = {
  description: string,
  id: string,
  name: string,
  quantity: int,
  premise_id: string,
  [@json.option]
  period_list: option(Model.Pricing.period_list),
};

[@deriving json]
type inventory_payload = {
  description: string,
  id: string,
  name: string,
  quantity: int,
  premise_id: string,
  [@json.option]
  period_list: option(Model.Pricing.period_list),
};

[@deriving json]
type config_payload = {
  [@json.option]
  inventory: option(array(inventory_payload)),
  [@json.option]
  premise: option(PeriodList.Premise.t),
};

type patch =
  | InventoryUpsert(inventory_patch_data)
  | InventoryDelete(string);

[@deriving json]
type payload = {
  config: config_payload,
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

let stateElementId = "initial-store";

let derivePeriodList = (config: Model.t) => {
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
  |> Array.of_list;
};

let payloadOfConfig = (config: config): payload => {
  config: {
    inventory:
      Some(
        config.inventory
        |> Array.map((item: Model.InventoryItem.t): inventory_payload => {
             description: item.description,
             id: item.id,
             name: item.name,
             quantity: item.quantity,
             premise_id: item.premise_id,
             period_list: Some(item.period_list),
           }),
      ),
    premise: config.premise,
  },
  unit:
    switch%platform (Runtime.platform) {
    | Server => PeriodList.Unit.defaultState
    | Client => PeriodList.Unit.get()
    },
};

let configOfPayload = (payload: payload): config => {
  inventory:
    switch (payload.config.inventory) {
    | Some(inventory) =>
      inventory
      |> Array.map((item: inventory_payload): Model.InventoryItem.t => {
           description: item.description,
           id: item.id,
           name: item.name,
           quantity: item.quantity,
           premise_id: item.premise_id,
           period_list:
             switch (item.period_list) {
             | Some(periodList) => periodList
             | None => [||]
             },
         })
    | None => [||]
    },
  premise: payload.config.premise,
};

let project = (config: config): projections => {
  premise_id:
    switch (config.premise) {
    | None => ""
    | Some(premise) => premise.id
    },
  period_list: derivePeriodList(config),
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

let emptyStore: store = {
  premise_id: "",
  config: Model.SSR.empty,
  period_list: [||],
  unit: PeriodList.Unit.defaultState,
};

let find_existing_period_list = (currentConfig: config, itemId: string) => {
  switch (
    Js.Array.find(
      ~f=(i: Model.InventoryItem.t) => i.id === itemId,
      currentConfig.inventory,
    )
  ) {
  | Some(existingItem) => existingItem.period_list
  | None => [||]
  };
};

let item_of_patch =
    (currentConfig: config, item: inventory_patch_data)
    : Model.InventoryItem.t => {
  let period_list =
    switch (item.period_list) {
    | Some(period_list) => period_list
    | None => find_existing_period_list(currentConfig, item.id)
    };

  {
    Model.InventoryItem.description: item.description,
    id: item.id,
    name: item.name,
    quantity: item.quantity,
    premise_id: item.premise_id,
    period_list,
  };
};

let decodePatch =
  StorePatch.compose([
    StorePatch.Pg.decodeAs(
      ~table="inventory",
      ~decodeRow=inventory_patch_data_of_json,
      ~insert=data => InventoryUpsert(data),
      ~update=data => InventoryUpsert(data),
      ~delete=id => InventoryDelete(id),
      (),
    ),
  ]);

let updateInventory = (currentConfig: config, newItem: inventory_patch_data): config => {
  let itemWithPeriod = item_of_patch(currentConfig, newItem);
  let exists =
    currentConfig.inventory
    |> Js.Array.some(~f=(i: Model.InventoryItem.t) => i.id === newItem.id);
  let newInventory =
    if (exists) {
      currentConfig.inventory
      |> Js.Array.map(~f=(i: Model.InventoryItem.t) =>
           i.id === itemWithPeriod.id ? itemWithPeriod : i
          );
    } else {
      Array.append(currentConfig.inventory, [|itemWithPeriod|]);
    };
  {
    ...currentConfig,
    inventory: newInventory,
  };
};

let deleteInventory = (currentConfig: config, id: string): config => {
  let newInventory =
    currentConfig.inventory
    |> Js.Array.filter(~f=(i: Model.InventoryItem.t) => i.id !== id);
  {
    ...currentConfig,
    inventory: newInventory,
  };
};

let updateOfPatch = patch =>
  switch (patch) {
  | InventoryUpsert(newItem) => currentConfig => updateInventory(currentConfig, newItem)
  | InventoryDelete(id) => currentConfig => deleteInventory(currentConfig, id)
  };

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

module Runtime = StoreBuilder.Runtime.Make({
  type nonrec config = config;
  type nonrec patch = patch;
  type nonrec payload = payload;
  type nonrec store = store;
  type nonrec subscription = subscription;

  let emptyStore = emptyStore;
  let stateElementId = stateElementId;
  let payloadOfConfig = payloadOfConfig;
  let configOfPayload = configOfPayload;
  let makeStore = makeStore;
  let config_of_json = config_of_json;
  let config_to_json = config_to_json;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
  let decodePatch = decodePatch;
  let subscriptionOfConfig = subscriptionOfConfig;
  let encodeSubscription = encodeSubscription;
  let updatedAtOf = updatedAtOf;
  let updateOfPatch = updateOfPatch;
  let eventUrl = eventUrl;
  let baseUrl = baseUrl;
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
