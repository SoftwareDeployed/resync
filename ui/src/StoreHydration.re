/* StoreHydration.re - Handles client-side hydration of Store from DOM JSON */

// Empty config fallback
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
  let dict = (Obj.magic(json): Js.Dict.t(Js.Json.t));
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
  let dict = (Obj.magic(json): Js.Dict.t(Js.Json.t));
  {
    Config.InventoryItem.description: (Obj.magic(Js.Dict.unsafeGet(dict, "description")): string),
    id: (Obj.magic(Js.Dict.unsafeGet(dict, "id")): int),
    name: (Obj.magic(Js.Dict.unsafeGet(dict, "name")): string),
    quantity: (Obj.magic(Js.Dict.unsafeGet(dict, "quantity")): int),
    premise_id: (Obj.magic(Js.Dict.unsafeGet(dict, "premise_id")): string),
    period_list: Array.map(period_from_json, (Obj.magic(Js.Dict.unsafeGet(dict, "period_list")): array(Js.Json.t))),
  };
};

let premise_from_json = (json: Js.Json.t) => {
  let dict = (Obj.magic(json): Js.Dict.t(Js.Json.t));
  let timestamp = (Obj.magic(Js.Dict.unsafeGet(dict, "updated_at")): float);
  {
    PeriodList.Premise.id: (Obj.magic(Js.Dict.unsafeGet(dict, "id")): string),
    name: (Obj.magic(Js.Dict.unsafeGet(dict, "name")): string),
    description: (Obj.magic(Js.Dict.unsafeGet(dict, "description")): string),
    updated_at: Js.Date.fromFloat(timestamp),
  };
};

let config_from_json = (json: Js.Json.t) => {
  let dict = (Obj.magic(json): Js.Dict.t(Js.Json.t));
  let premise_json = Js.Dict.get(dict, "premise");
  {
    Config.inventory: Array.map(inventory_item_from_json, (Obj.magic(Js.Dict.unsafeGet(dict, "inventory")): array(Js.Json.t))),
    premise: switch (premise_json) {
    | None => None
    | Some(p) =>
      if (p == Js.Json.null) {
        None
      } else {
        Some(premise_from_json(p))
      }
    },
  };
};

/* Parse minimal store JSON {config, unit} and return Config.t and Unit.t */
let parseMinimalStore = () =>
  try({
    let element = [%raw {| document.getElementById("initial-store") |}];
    switch (Js.Nullable.toOption(element)) {
    | Some(_) =>
      let text = [%raw {| document.getElementById("initial-store").textContent |}];
      let json = text->Js.Json.parseExn;
      let dict = (Obj.magic(json): Js.Dict.t(Js.Json.t));
      let config = config_from_json(Js.Dict.unsafeGet(dict, "config"));
      let unit_str = (Obj.magic(Js.Dict.unsafeGet(dict, "unit")): string);
      let unit = unit_from_string(unit_str);
      Some((config, unit));
    | None => Some((emptyConfig, PeriodList.Unit.defaultState))
    };
  }) {
  | _ => Some((emptyConfig, PeriodList.Unit.defaultState))
  };

/* Hydrate store from DOM - returns Tilia-wrapped Store.t on client */
let hydrateStore = () =>
  switch%platform (Runtime.platform) {
  | Client =>
    switch (parseMinimalStore()) {
    | None => Store.empty
    | Some((config, unit)) =>
      // Create Tilia-wrapped store with reactive derived fields
      Store.makeStore(~config, ~unit)
    }
  | Server =>
    // On server, we shouldn't be calling hydrateStore
    // The store is created in EntryServer and passed via Context
    Store.empty
  };
