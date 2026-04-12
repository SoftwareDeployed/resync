open Lwt.Syntax;
open Caqti_request.Infix;
module type DB = Caqti_lwt.CONNECTION;
module R = Caqti_request;
module T = Caqti_type;

let period_list_type = Realtime_schema_caqti.json_text(Model.Pricing.period_list_of_json);

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
      (T.string ->* inventory_item_caqti_type)(RealtimeSchema.Queries.GetInventoryList.sql)
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
      (T.string ->? inventory_item_caqti_type)(RealtimeSchema.Queries.GetCompleteInventory.sql)
    );
  (module Db: DB) => {
    let* item_or_error = Db.find_opt(query, item_id);
    Caqti_lwt.or_fail(item_or_error);
  };
};

let update_quantity = (item_id: string, quantity: int) => {
  let query =
    Caqti_request.Infix.(
      (T.t2(T.string, T.int) ->. T.unit)(
        "UPDATE inventory SET quantity = $2 WHERE id = $1"
      )
    );
  (module Db: DB) => {
    let* result = Db.exec(query, (item_id, quantity));
    Caqti_lwt.or_fail(result);
  };
};
