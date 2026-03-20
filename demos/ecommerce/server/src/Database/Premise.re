open Lwt.Syntax;
open Caqti_request.Infix;
module type DB = Caqti_lwt.CONNECTION;
module R = Caqti_request;
module T = Caqti_type;

let date_type =
  T.custom(
    ~encode=(date => Ok(Js.Date.getTime(date))),
    ~decode=(ts => Ok(Js.Date.fromFloat(ts))),
    T.float,
  );

let premise_caqti_type =
  T.product((id, name, description, updated_at) => {
    PeriodList.Premise.id: id,
    name: name,
    description: description,
    updated_at: updated_at,
  })
    @@ T.proj(T.string, ((premise: PeriodList.Premise.t) => premise.id))
    @@ T.proj(T.string, ((premise: PeriodList.Premise.t) => premise.name))
    @@ T.proj(T.string, ((premise: PeriodList.Premise.t) => premise.description))
    @@ T.proj(date_type, ((premise: PeriodList.Premise.t) => premise.updated_at))
    @@ T.proj_end;

let get_route_premise = (route_root: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->? premise_caqti_type)(
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
    Caqti_lwt.or_fail(premise_or_error);
  };
};

let get_premise = (premise_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->? premise_caqti_type)(
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
    Caqti_lwt.or_fail(premise_or_error);
  };
};
