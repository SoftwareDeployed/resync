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
  [@platform js]
  module JsonDecode = {
    let object_ = Js.Json.decodeObject;
    let string = Js.Json.decodeString;
    let number = Js.Json.decodeNumber;
    let array = Js.Json.decodeArray;
  };

  [@platform native]
  module JsonDecode = {
    let unavailable = feature =>
      failwith(feature ++ " is only available on the client");

    let object_ = (_: Js.Json.t) => unavailable("Js.Json.decodeObject");
    let string = (_: Js.Json.t) => unavailable("Js.Json.decodeString");
    let number = (_: Js.Json.t) => unavailable("Js.Json.decodeNumber");
    let array = (_: Js.Json.t) => unavailable("Js.Json.decodeArray");
  };

  let decode_error = (context, message) => failwith(context ++ ": " ++ message);

  let object_from_json = (context, json: Js.Json.t) =>
    switch (JsonDecode.object_(json)) {
    | Some(dict) => dict
    | None => decode_error(context, "expected object")
    };

  let decode_string = (context, field, json: Js.Json.t) =>
    switch (JsonDecode.string(json)) {
    | Some(value) => value
    | None => decode_error(context, "expected string for `" ++ field ++ "`")
    };

  let decode_number = (context, field, json: Js.Json.t) =>
    switch (JsonDecode.number(json)) {
    | Some(value) => value
    | None => decode_error(context, "expected number for `" ++ field ++ "`")
    };

  let decode_int = (context, field, json: Js.Json.t) => {
    let value = decode_number(context, field, json);
    let int_value = int_of_float(value);
    if (Float.equal(float_of_int(int_value), value)) {
      int_value;
    } else {
      decode_error(context, "expected integer for `" ++ field ++ "`");
    };
  };

  let required_field = (context, dict, field) =>
    switch (Js.Dict.get(dict, field)) {
    | Some(value) => value
    | None => decode_error(context, "missing required field `" ++ field ++ "`")
    };

  let required_string_field = (context, dict, field) =>
    decode_string(context, field, required_field(context, dict, field));

  let required_int_field = (context, dict, field) =>
    decode_int(context, field, required_field(context, dict, field));

  let required_number_field = (context, dict, field) =>
    decode_number(context, field, required_field(context, dict, field));

  let required_array_field = (context, dict, field) =>
    switch (JsonDecode.array(required_field(context, dict, field))) {
    | Some(values) => values
    | None => decode_error(context, "expected array for `" ++ field ++ "`")
    };

  let optional_string_field = (context, dict, field) =>
    switch (Js.Dict.get(dict, field)) {
    | Some(value) => Some(decode_string(context, field, value))
    | None => None
    };

  let optional_array_field = (context, dict, field) =>
    switch (Js.Dict.get(dict, field)) {
    | Some(value) =>
      switch (JsonDecode.array(value)) {
      | Some(values) => Some(values)
      | None => decode_error(context, "expected array for `" ++ field ++ "`")
      }
    | None => None
    };

  let period_from_json = (json: Js.Json.t) => {
    let context = "Config.Pricing.period";
    let dict = object_from_json(context, json);
    {
      Config.Pricing.id: required_int_field(context, dict, "id"),
      unit: required_string_field(context, dict, "unit"),
      label: required_string_field(context, dict, "label"),
      price: required_int_field(context, dict, "price"),
      max_value: required_int_field(context, dict, "max_value"),
      min_value: required_int_field(context, dict, "min_value"),
    };
  };

  let inventory_item_from_json = (json: Js.Json.t) => {
    let context = "Config.InventoryItem";
    let dict = object_from_json(context, json);
    {
      Config.InventoryItem.description: required_string_field(context, dict, "description"),
      id: required_string_field(context, dict, "id"),
      name: required_string_field(context, dict, "name"),
      quantity: required_int_field(context, dict, "quantity"),
      premise_id: required_string_field(context, dict, "premise_id"),
      period_list:
        Array.map(
          period_from_json,
          required_array_field(context, dict, "period_list"),
        ),
    };
  };

  let premise_from_json = (json: Js.Json.t) => {
    let context = "PeriodList.Premise";
    let dict = object_from_json(context, json);
    let timestamp = required_number_field(context, dict, "updated_at");
    {
      PeriodList.Premise.id: required_string_field(context, dict, "id"),
      name: required_string_field(context, dict, "name"),
      description: required_string_field(context, dict, "description"),
      updated_at: Js.Date.fromFloat(timestamp),
    };
  };

  let config_from_json = (json: Js.Json.t) => {
    let context = "Config";
    let dict = object_from_json(context, json);
    let premise_json = Js.Dict.get(dict, "premise");
    let inventory_json = required_array_field(context, dict, "inventory");
    {
      Config.inventory:
        Array.map(inventory_item_from_json, inventory_json),
      premise:
        switch (premise_json) {
        | None => None
        | Some(p) =>
          switch%platform (Runtime.platform) {
          | Client =>
            if (p === Js.Json.null) {
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

let unit_from_string = (s: string) =>
  switch (PeriodList.Unit.tFromJs(s)) {
  | Some(u) => u
  | None => PeriodList.Unit.defaultState
  };

let decodeState = (json: Js.Json.t): payload => {
  let dict = Codec.object_from_json("Store payload", json);
  let config = Codec.config_from_json(json);
  let unit =
    switch (Js.Dict.get(dict, "unit")) {
    | Some(unitJson) =>
      unit_from_string(Codec.decode_string("Store payload", "unit", unitJson))
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

let inventory_item_from_patch_json = (json: Js.Json.t) => {
  let context = "Inventory patch data";
  let dict = Codec.object_from_json(context, json);
  let period_list =
    switch (Codec.optional_array_field(context, dict, "period_list")) {
    | Some(periods) => Array.map(Codec.period_from_json, periods)
    | None => [||]
    };
  {
    Config.InventoryItem.description: Codec.required_string_field(context, dict, "description"),
    id: Codec.required_string_field(context, dict, "id"),
    name: Codec.required_string_field(context, dict, "name"),
    quantity: Codec.required_int_field(context, dict, "quantity"),
    premise_id: Codec.required_string_field(context, dict, "premise_id"),
    period_list,
  };
};

let parsePatch = (json: Js.Json.t): option(patch) =>
  try({
    let dict = Codec.object_from_json("Patch", json);
    switch (Codec.optional_string_field("Patch", dict, "type")) {
    | Some(typeStr) =>
      if (String.equal(typeStr, "patch")) {
        let table_ =
          switch (Codec.optional_string_field("Patch", dict, "table")) {
          | Some(t) => t
          | None => ""
          };
        let action =
          switch (Codec.optional_string_field("Patch", dict, "action")) {
          | Some(t) => t
          | None => ""
          };
        let data =
          switch (Js.Dict.get(dict, "data")) {
          | Some(d) => Some(inventory_item_from_patch_json(d))
          | None => None
          };
        let id = Codec.optional_string_field("Patch", dict, "id");
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

/* 0.0 means no premise snapshot is loaded yet. */
let updatedAtOf = (config: config) =>
  switch (config.premise) {
  | Some(premise) => premise.updated_at->Js.Date.getTime
  | None => 0.0
  };

let decodeSnapshot = (data: string): config => Codec.config_from_string(data);
let eventUrl = Constants.event_url;
let baseUrl = RealtimeClient.Socket.defaultBaseUrl;
