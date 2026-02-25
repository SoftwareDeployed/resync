open Caqti_request.Infix;
module type DB = Caqti_lwt.CONNECTION;
module R = Caqti_request;
module T = Caqti_type;

type t = {
  id: string,
  name: string,
  description: string,
  updated_at: Js.Date.t,
};

let get_route_premise = (route_root: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->? T.(t4(string, string, string, float)))(
        {sql|
          SELECT
            premise.id,
            premise.name,
            premise.description,
            EXTRACT(EPOCH FROM premise.updated_at) AS updated_at
          FROM premise_route
          LEFT JOIN premise ON premise.id = premise_route.premise_id
          WHERE premise_route.route_root = $1
          LIMIT 1
        |sql},
      )
    );

  (module Db: DB) => {
    let%lwt premise_or_error = Db.find_opt(query, route_root);
    let%lwt premise_opt = Caqti_lwt.or_fail(premise_or_error);

    premise_opt
    |> Belt.Option.map(((id, name, description, updated_at_seconds)) => {
         {
           id,
           name,
           description,
           updated_at: Js.Date.fromFloat(updated_at_seconds *. 1000.0),
         };
       })
    |> Lwt.return;
  };
};

let get_premise = (premise_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.unit ->* T.(t4(string, string, string, int)))(
        "SELECT * FROM premise WHERE id = :id",
      )
    );
  (module Db: DB) => {
    let%lwt premise_or_error = Db.collect_list(query, ());
    Caqti_lwt.or_fail(premise_or_error);
  };
};

/*
 let getConfig = (premise_id: string): Js.promise(Config.t) => {
   getPremise(premise_id)
   |> Js.Promise.then_(premise => {
        Inventory.getInventoryList(premise_id)
        |> Js.Promise.then_(inventory => {
             let config: Config.t = {
               inventory,
               premise: Some(premise),
             };
             Js.Promise.resolve(config);
           })
      });
 };
 */
