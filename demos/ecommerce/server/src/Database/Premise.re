open Lwt.Syntax;
open Caqti_request.Infix;
module type DB = Caqti_lwt.CONNECTION;
module R = Caqti_request;
module T = Caqti_type;

/* Helper to convert DB tuple to Premise.t */
let tuple_to_premise = ((id, name, description, updated_at_ts)) => {
  {
    PeriodList.Premise.id,
    name,
    description,
    updated_at: Js.Date.fromFloat(updated_at_ts),
  };
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
    let* premise_or_error = Db.find_opt(query, route_root);
    let* premise_tuple = Caqti_lwt.or_fail(premise_or_error);
    let premise_option =
      switch (premise_tuple) {
      | Some(tuple) => Some(tuple_to_premise(tuple))
      | None => None
      };
    Lwt.return(premise_option);
  };
};

let get_premise = (premise_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->? T.(t4(string, string, string, float)))(
        {sql|
          SELECT
            premise.id,
            premise.name,
            premise.description,
            EXTRACT(EPOCH FROM premise.updated_at) AS updated_at
          FROM premise
          WHERE premise.id = $1
          LIMIT 1
        |sql},
      )
    );
  (module Db: DB) => {
    let* premise_or_error = Db.find_opt(query, premise_id);
    let* premise_tuple = Caqti_lwt.or_fail(premise_or_error);
    let premise_option =
      switch (premise_tuple) {
      | Some(tuple) => Some(tuple_to_premise(tuple))
      | None => None
      };
    Lwt.return(premise_option);
  };
};
