/* StoreHydration.re - Handles client-side hydration of Store from DOM JSON */

// Empty config fallback with guaranteed array
let emptyConfig: Config.t = {
  inventory: [||],
  premise: None,
};

/* Parse PeriodList.Unit from string */
let unit_from_string = (s: string) =>
  switch (PeriodList.Unit.tFromJs(s)) {
  | Some(u) => u
  | None => PeriodList.Unit.defaultState
  };

/* JSON deserialization helpers for minimal {config, unit} format */
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
        if (p == Js.Json.null) {
          None;
        } else {
          Some(premise_from_json(p));
        }
      },
  };
};

type parsedStore = {
  config: Config.t,
  period_list: array(Config.Pricing.period),
};

let parseMinimalStore = () =>
  try({
    let element = [%raw {| document.getElementById("initial-store") |}];
    switch (Js.Nullable.toOption(element)) {
    | Some(_) =>
      let text = [%raw
        {| document.getElementById("initial-store").textContent |}
      ];
      let json = text->Js.Json.parseExn;
      let config = config_from_json(json);
      Some(config);
    | None => None
    };
  }) {
  | _ => None
  };

/* Hydrate store from DOM - returns Tilia-wrapped Store.t on client */
let hydrateStore = () =>
  switch%platform (Runtime.platform) {
  | Client =>
    switch (parseMinimalStore()) {
    | None => Store.empty
    | Some(config) => Store.makeStore(~config)
    }
  | Server => Store.empty
  };
