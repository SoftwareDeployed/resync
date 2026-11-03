// QueryRegistry.re - Universal query registry for SSR and client
//
// On native (server SSR): Collects queries during render, executes them, returns results
// On JS (client): Client-side cache stubs (full implementation in QueryCache.re)

include QueryRegistryTypes;

// Native-only: SSR query collection

[@native_only]
type registry_state =
  | Collecting
  | Executed
  | Rendered;

[@native_only]
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

[@native_only]
type query_registry = {
  mutable state: registry_state,
  mutable queries: Hashtbl.t(string, registered_query_exec),
  mutable ordered_keys: array(string),
  mutable results: Hashtbl.t(string, Yojson.Safe.t),
  mutable errors: Hashtbl.t(string, string),
  db_connection: option(module Caqti_lwt.CONNECTION),
};

[@native_only]
let registry_key: Lwt.key(query_registry) = Lwt.new_key();

[@native_only]
let sync_registry_ref: ref(option(query_registry)) = ref(None);

[@native_only]
let current_registry = () =>
  switch (Lwt.get(registry_key)) {
  | Some(registry) => Some(registry)
  | None => sync_registry_ref^
  };

[@native_only]
let copyRegistry = (~target, ~source) => {
  target.state = source.state;
  target.queries = source.queries;
  target.ordered_keys = source.ordered_keys;
  target.results = source.results;
  target.errors = source.errors;
};

[@native_only]
let clearRegistry = registry => {
  registry.state = Collecting;
  registry.queries = Hashtbl.create(8);
  registry.ordered_keys = [||];
  registry.results = Hashtbl.create(8);
  registry.errors = Hashtbl.create(8);
};

[@native_only]
let withScopedRegistry = (~registry, ~f) => {
  let previousRegistry = sync_registry_ref^;
  sync_registry_ref := None;
  try({
    let result = Lwt.with_value(registry_key, Some(registry), f);
    sync_registry_ref := previousRegistry;
    result;
  }) {
  | error =>
    sync_registry_ref := previousRegistry;
    raise(error);
  };
};

[@native_only]
let lwtIterArraySerial = (items: array('a), f: 'a => Lwt.t(unit)) => {
  let rec loop = index =>
    if (index >= Array.length(items)) {
      Lwt.return();
    } else {
      Lwt.bind(f(items[index]), _ => loop(index + 1));
    };

  loop(0);
};

[@native_only]
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

[@native_only]
let rec iterAssoc = (entries: list((string, Yojson.Safe.t)), f) =>
  switch (entries) {
  | [] => ()
  | [entry, ...rest] =>
    f(entry);
    iterAssoc(rest, f);
  };

[@native_only]
let isLoadedResult = fields =>
  switch (assocOpt("_tag", fields)) {
  | Some(`String("Loaded")) => true
  | _ => false
  };

[@native_only]
let errorMessageOfResult = fields =>
  switch (assocOpt("_tag", fields), assocOpt("message", fields)) {
  | (Some(`String("Error")), Some(`String(message))) => Some(message)
  | _ => None
  };

[@native_only]
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

[@native_only]
let with_registry = (~db, ~f, ()) => {
  let f: unit => Lwt.t('a) = f;
  let registry = {
    state: Collecting,
    queries: Hashtbl.create(8),
    ordered_keys: [||],
    results: Hashtbl.create(8),
    errors: Hashtbl.create(8),
    db_connection: Some(db),
  };
  withScopedRegistry(~registry, ~f);
};

[@native_only]
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
      if (Hashtbl.mem(registry.errors, key)) {
        None;
      } else if (Hashtbl.mem(registry.queries, key)) {
        None;
      } else {
        registry.ordered_keys =
          registry.ordered_keys->Js.Array.concat(~other=[|key|]);
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

[@native_only]
let find_result = (~key: string): option(Yojson.Safe.t) =>
  switch (current_registry()) {
  | Some(registry) => Hashtbl.find_opt(registry.results, key)
  | None => None
  };

[@native_only]
let find_error = (~key: string): option(string) =>
  switch (current_registry()) {
  | Some(registry) => Hashtbl.find_opt(registry.errors, key)
  | None => None
  };

[@native_only]
let registered_query_count = (): int =>
  switch (current_registry()) {
  | Some(registry) => Hashtbl.length(registry.queries)
  | None => 0
  };

[@native_only]
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

[@native_only]
let get_loaded_results = (): array(loaded_query_result) => {
  switch (current_registry()) {
  | None => [||]
  | Some(registry) =>
    let decoded =
      registry.ordered_keys
      |> Js.Array.map(~f=key =>
          switch (Hashtbl.find_opt(registry.results, key)) {
          | Some(value) =>
          switch (decodeRows(~decode=StoreJson.ofSafe, value)) {
          | Some(rows) =>
            Some({
              key,
              channel: channelOfKey(key),
              rows,
            }: loaded_query_result)
          | None => None
          }
          | None => None
          }
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
[@native_only]
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

[@native_only]
let registryFromSerialized = (~jsonStr: string): query_registry => {
  let json = Yojson.Safe.from_string(jsonStr);
  let results = Hashtbl.create(8);
  let errors = Hashtbl.create(8);
  let orderedKeys = ref([||]);
  (switch (json) {
  | `Assoc(entries) =>
    iterAssoc(entries, ((key, value)) => {
      switch (value) {
      | `Assoc(fields) =>
        orderedKeys := orderedKeys.contents->Js.Array.concat(~other=[|key|]);
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
    ordered_keys: orderedKeys.contents,
    results,
    errors,
    db_connection: None,
  };
};

// Setup registry from QueryCache-format JSON without clearing it.
// Prefer the current Lwt request context when one exists; fall back to the
// synchronous registry for legacy render paths and native tests.
[@native_only]
let setup_registry_from_json = (~jsonStr: string): unit => {
  let registry = registryFromSerialized(~jsonStr);
  switch (Lwt.get(registry_key)) {
  | Some(currentRegistry) =>
    copyRegistry(~target=currentRegistry, ~source=registry);
    sync_registry_ref := None;
  | None => sync_registry_ref := Some(registry)
  };
};

[@native_only]
let clear_registry = () => {
  sync_registry_ref := None;
  switch (Lwt.get(registry_key)) {
  | Some(registry) => clearRegistry(registry)
  | None => ()
  };
};

// Create a temporary registry from QueryCache-format JSON and run f inside it
[@native_only]
let with_serialized = (~jsonStr: string, ~f: unit => 'a, ()): 'a => {
  let registry = registryFromSerialized(~jsonStr);
  withScopedRegistry(~registry, ~f);
};

[@native_only]
let with_serialized_lwt = (~jsonStr: string, ~f: unit => Lwt.t('a), ()): Lwt.t('a) => {
  let registry = registryFromSerialized(~jsonStr);
  withScopedRegistry(~registry, ~f);
};

// JS-only: Client stubs (full implementation in QueryCache.re)

[@melange_only]
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

[@melange_only]
let find_result = (~key as _) => None;

[@melange_only]
let find_error = (~key as _) => None;

[@melange_only]
let registered_query_count = () => 0;

[@melange_only]
let execute_queries = () => ();

[@melange_only]
let get_loaded_results = () => [||];

[@melange_only]
let with_registry = (~db as _, ~f, ()) => f();

// Note: registry_key not needed on JS platform

[@melange_only]
let serialize_for_cache = (): string => "{}";

[@melange_only]
let with_serialized = (~jsonStr as _, ~f, ()) => f();

[@melange_only]
let with_serialized_lwt = (~jsonStr as _, ~f, ()) => f();
