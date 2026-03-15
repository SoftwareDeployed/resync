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
