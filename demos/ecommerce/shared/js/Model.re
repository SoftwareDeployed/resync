open Melange_json.Primitives;

module Pricing = {
  [@deriving json]
  type period = {
    id: int,
    unit: string,
    label: string,
    price: int,
    max_value: int,
    min_value: int,
  };

  [@deriving json]
  type period_list = array(period);
};

module InventoryItem = {
  [@deriving json]
  type t = {
    description: string,
    id: string,
    name: string,
    quantity: int,
    premise_id: string,
    period_list: Pricing.period_list,
  };
};

[@deriving json]
type t = {
  inventory: array(InventoryItem.t),
  [@json.option] premise: option(PeriodList.Premise.t),
};

module SSR = {
  let empty: t = {
    premise: None,
    inventory: [||],
  };
};
