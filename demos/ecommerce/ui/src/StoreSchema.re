open Melange_json.Primitives;

[@deriving json]
type config = Config.t;

type subscription = RealtimeSubscription.t;

[@deriving json]
type inventory_patch_data = {
  description: string,
  id: string,
  name: string,
  quantity: int,
  premise_id: string,
  [@json.option] period_list: option(Config.Pricing.period_list),
};

[@deriving json]
type patch = {
  [@json.key "type"] type_: string,
  [@json.key "table"] table_: string,
  action: string,
  [@json.option] data: option(inventory_patch_data),
  [@json.option] id: option(string),
};

[@deriving json]
type payload = {
  config: config,
  unit: PeriodList.Unit.t,
};

type projections = {
  premise_id: string,
  period_list: array(Config.Pricing.period),
};

type store = {
  premise_id: string,
  config: Config.t,
  period_list: array(Config.Pricing.period),
  unit: PeriodList.Unit.t,
};

let stateElementId = "initial-store";

let derivePeriodList = (config: Config.t) => {
  config.inventory
  |> Array.to_list
  |> List.map((inv: Config.InventoryItem.t) => Array.to_list(inv.period_list))
  |> List.concat
  |> List.fold_left(((periods: list(Config.Pricing.period)), (period: Config.Pricing.period)) => {
       let current_unit = period.unit;
       if (
         List.exists(
           (existing: Config.Pricing.period) => {
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
     }, ([]: list(Config.Pricing.period)))
  |> List.rev
  |> Array.of_list;
};

let payloadOfConfig = (config: config): payload => {
  config,
  unit: PeriodList.Unit.defaultState,
};

let configOfPayload = (payload: payload) => payload.config;

let project = (config: config): projections => {
  premise_id:
    switch (config.premise) {
    | None => ""
    | Some(premise) => premise.id
    },
  period_list: derivePeriodList(config),
};

let makeStore = (~config: config, ~payload: payload): store =>
  switch%platform (Runtime.platform) {
  | Client =>
    Tilia.Core.carve(derive => {
      config,
      premise_id: derive.derived(store => project(store.config).premise_id),
      period_list: derive.derived(store => project(store.config).period_list),
      unit: payload.unit,
    })
  | Server => {
      let projections = project(config);
      {
        premise_id: projections.premise_id,
        config,
        period_list: projections.period_list,
        unit: payload.unit,
      };
    }
  };

let emptyStore: store = {
  premise_id: "",
  config: Config.SSR.empty,
  period_list: [||],
  unit: PeriodList.Unit.defaultState,
};

let find_existing_period_list = (currentConfig: config, itemId: string) => {
  switch (Js.Array.find(~f=(i: Config.InventoryItem.t) => i.id === itemId, currentConfig.inventory)) {
  | Some(existingItem) => existingItem.period_list
  | None => [||]
  };
};

let item_of_patch = (currentConfig: config, item: inventory_patch_data): Config.InventoryItem.t => {
  let period_list =
    switch (item.period_list) {
    | Some(period_list) => period_list
    | None => find_existing_period_list(currentConfig, item.id)
    };

  {
    Config.InventoryItem.description: item.description,
    id: item.id,
    name: item.name,
    quantity: item.quantity,
    premise_id: item.premise_id,
    period_list,
  };
};

let applyPatch = (currentConfig: config, patch: patch): config => {
  switch (patch.type_, patch.table_, patch.action) {
  | ("patch", "inventory", "INSERT" | "UPDATE") =>
    switch (patch.data) {
    | Some(newItem) =>
      let itemWithPeriod = item_of_patch(currentConfig, newItem);
      let exists =
        currentConfig.inventory
        |> Js.Array.some(~f=(i: Config.InventoryItem.t) => i.id === newItem.id);
      let newInventory =
        if (exists) {
          currentConfig.inventory
          |> Js.Array.map(~f=(i: Config.InventoryItem.t) =>
               i.id === itemWithPeriod.id ? itemWithPeriod : i
             );
        } else {
          Array.append(currentConfig.inventory, [|itemWithPeriod|]);
        };
      {
        ...currentConfig,
        inventory: newInventory,
      };
    | None => currentConfig
    }
  | ("patch", "inventory", "DELETE") =>
    switch (patch.id) {
    | Some(id) =>
      let newInventory =
        currentConfig.inventory
        |> Js.Array.filter(~f=(i: Config.InventoryItem.t) => i.id !== id);
      {
        ...currentConfig,
        inventory: newInventory,
      };
    | None => currentConfig
    }
  | _ => currentConfig
  };
};

let subscriptionOfConfig = (config: config): option(subscription) =>
  switch (config.premise) {
  | Some(premise) => Some(RealtimeSubscription.premise(premise.id))
  | None => None
  };

let encodeSubscription = RealtimeSubscription.encode;

/* 0.0 means no premise snapshot is loaded yet. */
let updatedAtOf = (config: config) =>
  switch (config.premise) {
  | Some(premise) => premise.updated_at->Js.Date.getTime
  | None => 0.0
  };

let eventUrl = Constants.event_url;
let baseUrl = RealtimeClient.Socket.defaultBaseUrl;
