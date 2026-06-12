// QueryRegistry.re - Universal query registry for SSR and client
//
// On native (server SSR): Collects queries during render, executes them, returns results
// On JS (client): Client-side cache stubs (full implementation in QueryCache.re)

include QueryRegistryTypes;

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
  mutable errors: Hashtbl.t(string, string),
  db_connection: option(module Caqti_lwt.CONNECTION),
};

[@platform native]
let registry_key: Lwt.key(query_registry) = Lwt.new_key();

[@platform native]
let sync_registry_ref: ref(option(query_registry)) = ref(None);

[@platform native]
let current_registry = () =>
  switch (Lwt.get(registry_key)) {
  | Some(registry) => Some(registry)
  | None => sync_registry_ref^
  };

[@platform native]
let lwtIterArraySerial = (items: array('a), f: 'a => Lwt.t(unit)) => {
  let rec loop = index =>
    if (index >= Array.length(items)) {
      Lwt.return();
    } else {
      Lwt.bind(f(items[index]), _ => loop(index + 1));
    };

  loop(0);
};

[@platform native]
let rec assocOpt = (key: string, fields: list((string, Yojson.Safe.t))) =>
  switch (fields) {
  | [] => None
  | [(currentKey, value), ...rest] =>
    if (currentKey == key) {
      Some(value);
    } else {
      assocOpt(key, rest);
    }
  };

[@platform native]
let rec iterAssoc = (entries: list((string, Yojson.Safe.t)), f) =>
  switch (entries) {
  | [] => ()
  | [entry, ...rest] =>
    f(entry);
    iterAssoc(rest, f);
  };

[@platform native]
let isLoadedResult = fields =>
  switch (assocOpt("_tag", fields)) {
  | Some(`String("Loaded")) => true
  | _ => false
  };

[@platform native]
let errorMessageOfResult = fields =>
  switch (assocOpt("_tag", fields), assocOpt("message", fields)) {
  | (Some(`String("Error")), Some(`String(message))) => Some(message)
  | _ => None
  };

[@platform native]
let decodeRows = (~decode, json): option(array('a)) =>
  try({
    let decodeList = items => {
      let rec length = (remaining, count) =>
        switch (remaining) {
        | [] => count
        | [_, ...rest] => length(rest, count + 1)
        };

      switch (items) {
      | [] => [||]
      | [first, ...rest] =>
        let rows = Array.make(length(items, 0), decode(first));
        let rec fill = (index, remaining) =>
          switch (remaining) {
          | [] => ()
          | [item, ...tail] =>
            rows[index] = decode(item);
            fill(index + 1, tail);
          };

        fill(1, rest);
        rows;
      };
    };

    let rows =
      switch (json) {
      | `List(items) => decodeList(items)
      | _ => [|decode(json)|]
      };
    Some(rows);
  }) {
  | _ => None
  };

[@platform native]
let with_registry = (~db, ~f, ()) => {
  let f: unit => Lwt.t('a) = f;
  let registry = {
    state: Collecting,
    queries: Hashtbl.create(8),
    results: Hashtbl.create(8),
    errors: Hashtbl.create(8),
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
  switch (current_registry()) {
  | None => None
  | Some(registry) =>
    switch (Hashtbl.find_opt(registry.results, key)) {
    | Some(json) => decodeRows(~decode, json)
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
let find_result = (~key: string): option(Yojson.Safe.t) =>
  switch (current_registry()) {
  | Some(registry) => Hashtbl.find_opt(registry.results, key)
  | None => None
  };

[@platform native]
let find_error = (~key: string): option(string) =>
  switch (current_registry()) {
  | Some(registry) => Hashtbl.find_opt(registry.errors, key)
  | None => None
  };

[@platform native]
let registered_query_count = (): int =>
  switch (current_registry()) {
  | Some(registry) => Hashtbl.length(registry.queries)
  | None => 0
  };

[@platform native]
let execute_queries = () => {
  switch (Lwt.get(registry_key)) {
  | None => Lwt.return()
  | Some(registry) =>
    switch (registry.state) {
    | Collecting =>
      let queries = Hashtbl.to_seq_values(registry.queries) |> Array.of_seq;
      lwtIterArraySerial(
        queries,
        (Query(q)) => {
          switch (registry.db_connection) {
          | None => Lwt.return()
          | Some((module Db)) =>
            Lwt.bind(q.execute((module Db)), result => {
              switch (result) {
              | Ok(json) => Hashtbl.replace(registry.results, q.key, json); Lwt.return()
              | Error(message) => Hashtbl.replace(registry.errors, q.key, message); Lwt.return()
              };
            });
          };
        },
      ) |> Lwt.map(_ => {
        registry.state = Executed;
      });
    | _ => Lwt.return()
    }
  };
};

[@platform native]
let get_loaded_results = (): array(loaded_query_result) => {
  switch (current_registry()) {
  | None => [||]
  | Some(registry) =>
    let decoded =
      Hashtbl.to_seq(registry.results)
      |> Array.of_seq
      |> Js.Array.map(~f=((key, value)) =>
          switch (decodeRows(~decode=StoreJson.ofSafe, value)) {
          | Some(rows) =>
            Some({
              key,
              channel: channelOfKey(key),
              rows,
            }: loaded_query_result)
          | None => None
          },
        );
    let count = ref(0);
    let first = ref(None);
    decoded->Js.Array.forEach(~f=result =>
      switch (result) {
      | Some(loadedResult) =>
        switch (first.contents) {
        | None => first := Some(loadedResult)
        | Some(_) => ()
        };
        count := count.contents + 1;
      | None => ()
      }
    );
    switch (first.contents) {
    | None => [||]
    | Some(firstResult) =>
      let results = Array.make(count.contents, firstResult);
      let index = ref(0);
      decoded->Js.Array.forEach(~f=result =>
        switch (result) {
        | Some(loadedResult) =>
          results[index.contents] = loadedResult;
          index := index.contents + 1;
        | None => ()
        }
      );
      results;
    };
  };
};

// Serialize registry results to QueryCache hydrate format
// {"channel:hash": {"_tag": "Loaded", "data": [...]}}
[@platform native]
let serialize_for_cache = (): string => {
  switch (Lwt.get(registry_key)) {
  | None => "{}"
  | Some(registry) =>
    let loadedEntries =
      Hashtbl.to_seq(registry.results)
      |> Array.of_seq
      |> Js.Array.map(~f=((key, value)) =>
          (
            key,
            `Assoc([("_tag", `String("Loaded")), ("data", value)]),
          )
      );
    let errorEntries =
      Hashtbl.to_seq(registry.errors)
      |> Array.of_seq
      |> Js.Array.map(~f=((key, message)) =>
          (
            key,
            `Assoc([("_tag", `String("Error")), ("message", `String(message))]),
          )
      );
    Yojson.Safe.to_string(
      `Assoc(StoreJson.listOfArray(
        loadedEntries->Js.Array.concat(~other=errorEntries),
      )),
    )
  };
};

[@platform native]
let registryFromSerialized = (~jsonStr: string): query_registry => {
  let json = Yojson.Safe.from_string(jsonStr);
  let results = Hashtbl.create(8);
  let errors = Hashtbl.create(8);
  (switch (json) {
  | `Assoc(entries) =>
    iterAssoc(entries, ((key, value)) => {
      switch (value) {
      | `Assoc(fields) =>
        if (isLoadedResult(fields)) {
          switch (assocOpt("data", fields)) {
          | Some(data) => Hashtbl.replace(results, key, data)
          | None => ()
          };
        } else {
          switch (errorMessageOfResult(fields)) {
          | Some(message) => Hashtbl.replace(errors, key, message)
          | None => ()
          };
        }
      | _ => ()
      };
    })
  | _ => ()
  });
  {
    state: Rendered,
    queries: Hashtbl.create(8),
    results,
    errors,
    db_connection: None,
  };
};

// Setup registry from QueryCache-format JSON without clearing it.
// Prefer the current Lwt request context when one exists; fall back to the
// synchronous registry for legacy render paths and native tests.
[@platform native]
let setup_registry_from_json = (~jsonStr: string): unit => {
  let registry = registryFromSerialized(~jsonStr);
  switch (Lwt.get(registry_key)) {
  | Some(currentRegistry) =>
    currentRegistry.state = registry.state;
    currentRegistry.queries = registry.queries;
    currentRegistry.results = registry.results;
    currentRegistry.errors = registry.errors;
  | None => sync_registry_ref := Some(registry)
  };
};

[@platform native]
let clear_registry = () => {
  sync_registry_ref := None;
};

// Create a temporary registry from QueryCache-format JSON and run f inside it
[@platform native]
let with_serialized = (~jsonStr: string, ~f: unit => 'a, ()): 'a => {
  let previousRegistry = sync_registry_ref^;
  setup_registry_from_json(~jsonStr);
  try({
    let result = f();
    sync_registry_ref := previousRegistry;
    result;
  }) {
  | error =>
    sync_registry_ref := previousRegistry;
    raise(error);
  };
};

[@platform native]
let with_serialized_lwt = (~jsonStr: string, ~f: unit => Lwt.t('a), ()): Lwt.t('a) => {
  let registry = registryFromSerialized(~jsonStr);
  Lwt.with_value(registry_key, Some(registry), f);
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
let find_result = (~key as _) => None;

[@platform js]
let find_error = (~key as _) => None;

[@platform js]
let registered_query_count = () => 0;

[@platform js]
let execute_queries = () => ();

[@platform js]
let get_loaded_results = () => [||];

[@platform js]
let with_registry = (~db as _, ~f, ()) => f();

// Note: registry_key not needed on JS platform

[@platform js]
let serialize_for_cache = (): string => "{}";

[@platform js]
let with_serialized = (~jsonStr as _, ~f, ()) => f();

[@platform js]
let with_serialized_lwt = (~jsonStr as _, ~f, ()) => f();
