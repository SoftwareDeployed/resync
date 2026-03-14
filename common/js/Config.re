module Pricing = {
  type period = {
    id: int,
    unit: string,
    label: string,
    price: int,
    max_value: int,
    min_value: int,
  };
  type period_list = array(period);
};

module InventoryItem = {
  type t = {
    description: string,
    id: int,
    name: string,
    quantity: int,
    premise_id: string,
    period_list: Pricing.period_list,
  };
};

type t = {
  inventory: array(InventoryItem.t),
  premise: option(PeriodList.Premise.t),
};

module SSR = {
  let empty: t = {
    premise: None,
    inventory: [||],
  };
};

let period_to_yojson = (p: Pricing.period) =>
  `Assoc([
    ("id", `Int(p.id)),
    ("unit", `String(p.unit)),
    ("label", `String(p.label)),
    ("price", `Int(p.price)),
    ("max_value", `Int(p.max_value)),
    ("min_value", `Int(p.min_value)),
  ]);

let inventory_item_to_yojson = (item: InventoryItem.t) =>
  `Assoc([
    ("description", `String(item.description)),
    ("id", `Int(item.id)),
    ("name", `String(item.name)),
    ("quantity", `Int(item.quantity)),
    ("premise_id", `String(item.premise_id)),
    ("period_list", `List(List.map(period_to_yojson, Array.to_list(item.period_list)))),
  ]);

let premise_to_yojson = (p: PeriodList.Premise.t) =>
  `Assoc([
    ("id", `String(p.id)),
    ("name", `String(p.name)),
    ("description", `String(p.description)),
    ("updated_at", `Float(p.updated_at->Js.Date.getTime)),
  ]);

let to_yojson = (config: t) =>
  `Assoc([
    ("inventory", `List(List.map(inventory_item_to_yojson, Array.to_list(config.inventory)))),
    ("premise",
      switch (config.premise) {
      | None => `Null
      | Some(p) => premise_to_yojson(p)
      }
    ),
  ]);
