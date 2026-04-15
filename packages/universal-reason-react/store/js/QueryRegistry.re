// QueryRegistry.re - Universal query registry for SSR and client
//
// On native (server SSR): Collects queries during render, executes them, returns results
// On JS (client): Client-side cache stubs (full implementation in QueryCache.re)

// Shared types (both platforms)

type query_key = string;

type query_result('row) =
  | Loading
  | Loaded(array('row))
  | Error(string);

type registry_snapshot = {
  queries: array(query_key),
  results: Js.Dict.t(StoreJson.json),
};

let makeKey = (~channel: string, ~paramsHash: string): query_key => {
  channel ++ ":" ++ paramsHash;
};

// Native-only: SSR query collection

[@platform native]
type registry_state =
  | Collecting
  | Executed
  | Rendered;

[@platform native]
type registered_query_exec =
  | Query({
      key: string,
      channel: string,
      params: 'p,
      sql: string,
      execute:
        (module Caqti_lwt.CONNECTION) =>
        Lwt.t(Stdlib.result(Yojson.Safe.t, string)),
      decode: Yojson.Safe.t => 'a,
    })
    : registered_query_exec;

[@platform native]
type query_registry = {
  mutable state: registry_state,
  mutable queries: Hashtbl.t(string, registered_query_exec),
  mutable results: Hashtbl.t(string, Yojson.Safe.t),
  db_connection: option(module Caqti_lwt.CONNECTION),
};

[@platform native]
let registry_key: Lwt.key(query_registry) = Lwt.new_key();

[@platform native]
let with_registry = (~db, ~f, ()) => {
  let f: unit => Lwt.t('a) = f;
  let registry = {
    state: Collecting,
    queries: Hashtbl.create(8),
    results: Hashtbl.create(8),
    db_connection: Some(db),
  };
  Lwt.with_value(registry_key, Some(registry), f);
};

[@platform native]
let register_query =
    (
      ~key: string,
      ~channel: string,
      ~params: 'p,
      ~sql: string,
      ~execute,
      ~decode,
    )
    : option(array('a)) => {
  switch (Lwt.get(registry_key)) {
  | None => None
  | Some(registry) =>
    switch (Hashtbl.find_opt(registry.results, key)) {
    | Some(json) =>
      try(Some([|decode(json)|])) {
      | _ => None
      }
    | None =>
      if (Hashtbl.mem(registry.queries, key)) {
        None;
      } else {
        Hashtbl.add(
          registry.queries,
          key,
          Query({
            key,
            channel,
            params,
            sql,
            execute,
            decode,
          }),
        );
        None;
      }
    }
  };
};

[@platform native]
let execute_queries = () => {
  switch (Lwt.get(registry_key)) {
  | None => Lwt.return()
  | Some(registry) =>
    switch (registry.state) {
    | Collecting =>
      let queries = Hashtbl.to_seq_values(registry.queries) |> List.of_seq;
      Lwt_list.iter_p(
        (Query(q)) => {
          switch (registry.db_connection) {
          | None => Lwt.return()
          | Some((module Db)) =>
            Lwt.bind(q.execute((module Db)), result => {
              switch (result) {
              | Ok(json) => Hashtbl.replace(registry.results, q.key, json); Lwt.return()
              | Error(_) => Lwt.return()
              };
            });
          };
        },
        queries,
      ) |> Lwt.map(_ => {
        registry.state = Executed;
      });
    | _ => Lwt.return()
    }
  };
};

[@platform native]
let get_results = () => {
  switch (Lwt.get(registry_key)) {
  | None => { queries: [||], results: Js.Dict.empty() }
  | Some(registry) =>
    let queries = Hashtbl.to_seq_keys(registry.queries) |> Array.of_seq;
    let results = Js.Dict.empty();
    Hashtbl.iter((k, v) => results->Js.Dict.set(k, Obj.magic(v)), registry.results);
    { queries, results };
  };
};

// Serialize snapshot to JSON string for SSR
[@platform native]
let serialize_snapshot = (snapshot: registry_snapshot): string => {
  let jsonObj = `Assoc([
    ("queries", `List(snapshot.queries |> Array.to_list |> List.map(q => `String(q)))),
    ("results", `Assoc(
      snapshot.results
      |> Js.Dict.entries
      |> Array.to_list
      |> List.map(((k, v)) => (k, Obj.magic(v)))
    ))
  ]);
  Yojson.Basic.to_string(jsonObj);
};

// JS-only: Client stubs (full implementation in QueryCache.re)

[@platform js]
let register_query =
    (
      ~key as _,
      ~channel as _,
      ~params as _,
      ~sql as _,
      ~execute as _,
      ~decode as _,
    ) =>
  None;

[@platform js]
let execute_queries = () => ();

[@platform js]
let get_results = () => {
  queries: [||],
  results: Js.Dict.empty(),
};

[@platform js]
let with_registry = (~db as _, ~f, ()) => f();

[@platform js]
let serialize_snapshot = (_snapshot: registry_snapshot): string => "{}";

// Note: registry_key not needed on JS platform
