open Lwt.Syntax;
open Caqti_request.Infix;
module type DB = Caqti_lwt.CONNECTION;
module R = Caqti_request;
module T = Caqti_type;

/* Helper to parse JSON period list - returns empty array on any error */
let parse_period_list = (json_str: string) => {
  let result =
    try(Some(Yojson.Safe.from_string(json_str))) {
    | _ => None
    };

  switch (result) {
  | Some(`List(periods)) =>
    periods
    |> List.map(p => {
         switch (p) {
         | `Assoc(fields) =>
           let get_field = name =>
             switch (List.assoc_opt(name, fields)) {
             | Some(`String(s)) => s
             | Some(`Int(i)) => Int.to_string(i)
             | _ => ""
             };
           {
             Config.Pricing.id: 0,
             unit: get_field("unit"),
             label: get_field("label"),
             price:
               int_of_string_opt(get_field("price"))
               |> Option.value(~default=0),
             max_value:
               int_of_string_opt(get_field("max_value"))
               |> Option.value(~default=0),
             min_value:
               int_of_string_opt(get_field("min_value"))
               |> Option.value(~default=0),
           };
         | _ => {
             Config.Pricing.id: 0,
             unit: "",
             label: "",
             price: 0,
             max_value: 0,
             min_value: 0,
           }
         }
       })
    |> Array.of_list
  | _ => [||]
  };
};

/* Helper to convert DB tuple to InventoryItem.t with period_list */
let tuple_to_inventory_item =
    ((description, id, name, quantity, premise_id, period_list_json)) => {
  {
    Config.InventoryItem.description,
    id,
    name,
    quantity,
    premise_id,
    period_list: parse_period_list(period_list_json),
  };
};

let get_list = (premise_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->* T.(t6(string, string, string, int, string, string)))(
        {sql|
          SELECT
            i.description,
            i.id,
            i.name,
            i.quantity,
            i.premise_id,
            COALESCE(
              JSONB_AGG(
                TO_JSONB(p.*)
              ) FILTER (WHERE p.id IS NOT NULL),
              '[]'::jsonb
            )::text as period_list
          FROM inventory i
          JOIN inventory_period_map pm ON pm.inventory_id = i.id
          JOIN period p ON p.id = pm.period_id
          WHERE i.premise_id = $1
          GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity
        |sql},
      )
    );
  (module Db: DB) => {
    let* items_or_error = Db.collect_list(query, premise_id);
    let* items_list = Caqti_lwt.or_fail(items_or_error);
    let items =
      List.map(tuple_to_inventory_item, items_list) |> Array.of_list;
    Lwt.return(items);
  };
};

/* Get a single inventory item by ID with period_list */
let get_by_id = (item_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->? T.(t6(string, string, string, int, string, string)))(
        {sql|
          SELECT
            i.description,
            i.id,
            i.name,
            i.quantity,
            i.premise_id,
            COALESCE(
              JSONB_AGG(
                TO_JSONB(p.*)
              ) FILTER (WHERE p.id IS NOT NULL),
              '[]'::jsonb
            )::text as period_list
          FROM inventory i
          LEFT JOIN inventory_period_map pm ON pm.inventory_id = i.id
          LEFT JOIN period p ON p.id = pm.period_id
          WHERE i.id = $1
          GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity
        |sql},
      )
    );
  (module Db: DB) => {
    let* item_or_error = Db.find_opt(query, item_id);
    let* item_opt = Caqti_lwt.or_fail(item_or_error);
    switch (item_opt) {
    | Some(item) => Lwt.return(Some(tuple_to_inventory_item(item)))
    | None => Lwt.return(None)
    };
  };
};
