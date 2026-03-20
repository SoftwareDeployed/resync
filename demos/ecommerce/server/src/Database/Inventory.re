open Lwt.Syntax;
open Caqti_request.Infix;
module type DB = Caqti_lwt.CONNECTION;
module R = Caqti_request;
module T = Caqti_type;

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
             Model.Pricing.id: 0,
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
             Model.Pricing.id: 0,
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

let period_list_type =
  T.custom(
    ~encode=(_ => Error("encoding period_list not supported")),
    ~decode=(json_str => Ok(parse_period_list(json_str))),
    T.string,
  );

let inventory_item_caqti_type =
  T.product((description, id, name, quantity, premise_id, period_list) => {
    Model.InventoryItem.description: description,
    id: id,
    name: name,
    quantity: quantity,
    premise_id: premise_id,
    period_list: period_list,
  })
    @@ T.proj(T.string, ((item: Model.InventoryItem.t) => item.description))
    @@ T.proj(T.string, ((item: Model.InventoryItem.t) => item.id))
    @@ T.proj(T.string, ((item: Model.InventoryItem.t) => item.name))
    @@ T.proj(T.int, ((item: Model.InventoryItem.t) => item.quantity))
    @@ T.proj(T.string, ((item: Model.InventoryItem.t) => item.premise_id))
    @@ T.proj(period_list_type, ((item: Model.InventoryItem.t) => item.period_list))
    @@ T.proj_end;

let get_list = (premise_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->* inventory_item_caqti_type)(
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
    Lwt.return(Array.of_list(items_list));
  };
};

let get_by_id = (item_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->? inventory_item_caqti_type)(
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
    Caqti_lwt.or_fail(item_or_error);
  };
};
