type input_premise = Js.Dict.t(string);

type input_config = {
  inventory: array(Config.InventoryItem.t),
  premise: option(input_premise),
};

let empty: Config.t = {
  inventory: [||],
  premise: None,
};

/* JSON deserialization helpers for client-side hydration */
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
    id: (Obj.magic(Js.Dict.unsafeGet(dict, "id")): int),
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
  {
    Config.inventory:
      Array.map(
        inventory_item_from_json,
        Obj.magic(Js.Dict.unsafeGet(dict, "inventory")): array(Js.Json.t),
      ),
    premise:
      switch (premise_json) {
      | None => None
      | Some(p) =>
        if (p == Js.Json.null) {
          None;
        } else {
          Some(premise_from_json(p));
        }
      },
  };
};

let domExecutorConfig =
  switch%platform (Runtime.platform) {
  | Client =>
    try({
      let element = [%raw {| document.getElementById("initial-config") |}];
      switch (Js.Nullable.toOption(element)) {
      | Some(_) =>
        let text = [%raw
          {| document.getElementById("initial-config").textContent |}
        ];
        let json = text->Js.Json.parseExn;
        let config = config_from_json(json);
        Some(config);
      | None => Some(empty)
      };
    }) {
    | _ => Some(empty)
    }
  | Server => None
  };

let initialExecutorConfig =
  switch (domExecutorConfig) {
  | None => empty
  | Some(config) => config
  };

let state =
  switch%platform (Runtime.platform) {
  | Client =>
    Tilia.Core.source(. initialExecutorConfig, (. _prev, set) => {
      switch (initialExecutorConfig.premise) {
      | Some(premise) =>
        let { id, updated_at, _ }: PeriodList.Premise.t = premise;
        set->Client.Socket.subscribe(id, updated_at->Js.Date.getTime);
        initialExecutorConfig;
      | None => initialExecutorConfig
      }
    })
  | Server => initialExecutorConfig
  };
