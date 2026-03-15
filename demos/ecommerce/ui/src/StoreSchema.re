type config = Config.t;
type subscription = RealtimeSubscription.t;

type patch = {
  type_: string,
  table_: string,
  action: string,
  data: option(Config.InventoryItem.t),
  id: option(string),
};

type payload = {
  config: Config.t,
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

module Codec = {
  let period_from_json = (json: Js.Json.t) => {
    let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
    {
      Config.Pricing.id: (Obj.magic(Js.Dict.unsafeGet(dict, "id")): int),
      unit: (Obj.magic(Js.Dict.unsafeGet(dict, "unit")): string),
      label: (Obj.magic(Js.Dict.unsafeGet(dict, "label")): string),
      price: (Obj.magic(Js.Dict.unsafeGet(dict, "price")): int),
      max_value: (Obj.magic(Js.Dict.unsafeGet(dict, "max_value")): int),
      min_value: (Obj.magic(Js.Dict.unsafeGet(dict, "min_value")): int),
    };
  };

  let inventory_item_from_json = (json: Js.Json.t) => {
    let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
    {
      Config.InventoryItem.description: (
        Obj.magic(Js.Dict.unsafeGet(dict, "description")): string
      ),
      id: (Obj.magic(Js.Dict.unsafeGet(dict, "id")): string),
      name: (Obj.magic(Js.Dict.unsafeGet(dict, "name")): string),
      quantity: (Obj.magic(Js.Dict.unsafeGet(dict, "quantity")): int),
      premise_id: (Obj.magic(Js.Dict.unsafeGet(dict, "premise_id")): string),
      period_list:
        Array.map(
          period_from_json,
          Obj.magic(Js.Dict.unsafeGet(dict, "period_list")): array(Js.Json.t),
        ),
    };
  };

  let premise_from_json = (json: Js.Json.t) => {
    let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
    let timestamp: float = Obj.magic(Js.Dict.unsafeGet(dict, "updated_at"));
    {
      PeriodList.Premise.id: (
        Obj.magic(Js.Dict.unsafeGet(dict, "id")): string
      ),
      name: (Obj.magic(Js.Dict.unsafeGet(dict, "name")): string),
      description: (Obj.magic(Js.Dict.unsafeGet(dict, "description")): string),
      updated_at: Js.Date.fromFloat(timestamp),
    };
  };

  let config_from_json = (json: Js.Json.t) => {
    let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
    let premise_json = Js.Dict.get(dict, "premise");
    let inventory_json = Js.Dict.unsafeGet(dict, "inventory");
    {
      Config.inventory:
        Array.map(
          inventory_item_from_json,
          Obj.magic(inventory_json): array(Js.Json.t),
        ),
      premise:
        switch (premise_json) {
        | None => None
        | Some(p) =>
          switch%platform (Runtime.platform) {
          | Client =>
            if (p == Js.Json.null) {
              None;
            } else {
              Some(premise_from_json(p));
            }
          | Server => None
          }
        },
    };
  };

  let config_from_string = (data: string) =>
    switch%platform (Runtime.platform) {
    | Client => data->Js.Json.parseExn->config_from_json
    | Server =>
        let _ = data;
        {
          Config.inventory: [||],
          premise: None,
        }
    };
};

let derivePeriodList = (config: Config.t) => {
  let seen = ref([]);
  let periods = ref([]);
  config.inventory
  ->Belt.Array.forEach(inv => {
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

let unit_from_string = (s: string) =>
  switch (PeriodList.Unit.tFromJs(s)) {
  | Some(u) => u
  | None => PeriodList.Unit.defaultState
  };

let decodeState = (json: Js.Json.t): payload => {
  let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
  let config = Codec.config_from_json(json);
  let unit =
    switch (Js.Dict.get(dict, "unit")) {
    | Some(unitJson) => unit_from_string(Obj.magic(unitJson): string)
    | None => PeriodList.Unit.defaultState
    };
  {config, unit};
};

let encodeState = (payload: payload) => {
  let _ = payload;
  switch%platform (Runtime.platform) {
  | Server =>
    let period_list = derivePeriodList(payload.config);
    Yojson.Safe.to_string(`Assoc([
      (
        "inventory",
        `List(
          List.map(
            Config.inventory_item_to_yojson,
            Array.to_list(payload.config.inventory),
          ),
        ),
      ),
      (
        "premise",
        switch (payload.config.premise) {
        | None => `Null
        | Some(premise) => Config.premise_to_yojson(premise)
        },
      ),
      (
        "period_list",
        `List(List.map(Config.period_to_yojson, Array.to_list(period_list))),
      ),
      ("unit", `String(PeriodList.Unit.tToJs(payload.unit))),
    ]))
  | Client => ""
  };
};

let emptyStore: store = {
  premise_id: "",
  config: Config.SSR.empty,
  period_list: [||],
  unit: PeriodList.Unit.defaultState,
};

let inventory_item_from_json = (json: Js.Json.t) => {
  let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
  {
    Config.InventoryItem.description: (
      Obj.magic(Js.Dict.unsafeGet(dict, "description")): string
    ),
    id: (Obj.magic(Js.Dict.unsafeGet(dict, "id")): string),
    name: (Obj.magic(Js.Dict.unsafeGet(dict, "name")): string),
    quantity: (Obj.magic(Js.Dict.unsafeGet(dict, "quantity")): int),
    premise_id: (Obj.magic(Js.Dict.unsafeGet(dict, "premise_id")): string),
    period_list: [||],
  };
};

let parsePatch = (json: Js.Json.t): option(patch) =>
  try({
    let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
    switch (Js.Dict.get(dict, "type")) {
    | Some(typeJson) =>
      let typeStr: string = Obj.magic(typeJson);
      if (typeStr === "patch") {
        let table_ =
          switch (Js.Dict.get(dict, "table")) {
          | Some(t) => (Obj.magic(t): string)
          | None => ""
          };
        let action =
          switch (Js.Dict.get(dict, "action")) {
          | Some(t) => (Obj.magic(t): string)
          | None => ""
          };
        let data =
          switch (Js.Dict.get(dict, "data")) {
          | Some(d) => Some(inventory_item_from_json(d))
          | None => None
          };
        let id =
          switch (Js.Dict.get(dict, "id")) {
          | Some(i) => Some(Obj.magic(i): string)
          | None => None
          };
        Some({
          type_: "patch",
          table_,
          action,
          data,
          id,
        });
      } else {
        None;
      };
    | None => None
    };
  }) {
  | _ => None
  };

let find_existing_period_list = (currentConfig: config, itemId: string) => {
  switch (Js.Array.find(~f=(i: Config.InventoryItem.t) => i.id === itemId, currentConfig.inventory)) {
  | Some(existingItem) => existingItem.period_list
  | None => [||]
  };
};

let applyPatch = (currentConfig: config, patch: patch): config => {
  switch (patch.table_, patch.action) {
  | ("inventory", "INSERT" | "UPDATE") =>
    switch (patch.data) {
    | Some(newItem) =>
      let period_list = find_existing_period_list(currentConfig, newItem.id);
      let itemWithPeriod = {...newItem, period_list};
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
  | ("inventory", "DELETE") =>
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

let updatedAtOf = (config: config) =>
  switch (config.premise) {
  | Some(premise) => premise.updated_at->Js.Date.getTime
  | None => Js.Date.now()
  };

let decodeSnapshot = (data: string): config => Codec.config_from_string(data);
let eventUrl = Constants.event_url;
let baseUrl = RealtimeClient.Socket.defaultBaseUrl;
