open Melange_json.Primitives;

module Pricing = {
  [@deriving json]
  type period = {
    id: string,
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

module OrderItem = {
  [@deriving json]
  type t = {
    inventory_id: string,
    name: string,
    quantity: int,
    unit_label: string,
    unit_price_cents: int,
  };
};

module Order = {
  [@deriving json]
  type items = array(OrderItem.t);

  [@deriving json]
  type t = {
    id: string,
    premise_id: string,
    customer_name: string,
    customer_email: string,
    start_date: string,
    end_date: string,
    status: string,
    created_at: StoreJson.Date.t,
    items: items,
  };
};

[@deriving json]
type t = {
  inventory: array(InventoryItem.t),
  [@json.option] premise: option(PeriodList.Premise.t),
  [@json.option] order: option(Order.t),
};

module SSR = {
  let empty: t = {
    premise: None,
    order: None,
    inventory: [||],
  };
};
