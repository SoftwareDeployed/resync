// UseQuery.re - Universal React hook for declarative data fetching
//
// On native (SSR): Registers query with QueryRegistry during render,
// returns result after server execution
// On client (JS): Returns data from QueryCache, subscribes to WebSocket updates

open QueryRegistryTypes;

// Shared hook result type
type result('row) = {
  data: query_result('row),
  loading: bool,
  error: option(string),
};

// Global query cache (client-side)
let queryCacheRef: ref(option(QueryCache.t)) = ref(None);

// Get or create query cache
[@platform js]
let getQueryCache = (): QueryCache.t => {
  switch (queryCacheRef.contents) {
  | Some(cache) => cache
  | None =>
    let cache = QueryCache.make();
    queryCacheRef := Some(cache);
    cache;
  };
};

[@platform native]
let getQueryCache = (): QueryCache.t => {
  QueryCache.make();
};

// Initialize query cache with WebSocket URLs
[@platform js]
let initCache = (~eventUrl: string, ~baseUrl: string) => {
  switch (queryCacheRef.contents) {
  | Some(cache) => QueryCache.init(~eventUrl, ~baseUrl, cache)
  | None =>
    let cache = QueryCache.make();
    QueryCache.init(~eventUrl, ~baseUrl, cache);
    queryCacheRef := Some(cache);
  };
};

[@platform native]
let initCache = (~eventUrl as _: string, ~baseUrl as _: string) => ();

// Serialize cache for SSR hydration (native only - server serializes)
[@platform native]
let serializeCache = (): string => {
  let cache = getQueryCache();
  QueryCache.serialize(cache);
};

[@platform js]
let serializeCache = (): string => "{}";

// Hydrate cache from server-rendered data
[@platform js]
let hydrateCache = (jsonStr: string) => {
  let cache = getQueryCache();
  QueryCache.hydrate(~t=cache, ~jsonStr);
};

[@platform native]
let hydrateCache = (_jsonStr: string) => ();

// DOM element helpers for hydration
[@platform js]
type element;

[@platform js]
[@mel.scope "document"]
external getElementById: string => Js.Nullable.t(element) = "getElementById";

[@platform js]
[@mel.get]
external textContent: element => Js.Nullable.t(string) = "textContent";

// Hydrate cache from DOM script tag
[@platform js]
let hydrateCacheFromDom = (~cacheId="query-cache", ()) => {
  let cache = getQueryCache();
  switch (cacheId->getElementById->Js.Nullable.toOption) {
  | Some(element) =>
    switch (element->textContent->Js.Nullable.toOption) {
    | Some(text) => QueryCache.hydrate(~t=cache, ~jsonStr=text)
    | None => ()
    }
  | None => ()
  };
};

[@platform native]
let hydrateCacheFromDom = (~cacheId as _unused=?, ()) => {
  let _ = _unused;
  ();
};

[@platform js]
let useQuerySignal = (~cache, ~key, ~channel) => {
  let signal =
    React.useMemo1(() => QueryCache.getSignal(~t=cache, ~key), [|key|]);

  React.useEffect1(
    () => {
      let (_signal, unsubscribe) =
        QueryCache.subscribe(
          ~t=cache,
          ~key,
          ~channel,
          ~updatedAt=0.0,
          (),
        );
      Some(unsubscribe);
    },
    [|key|],
  );

  Tilia.React.useTilia();
  signal;
};

// Main useQuery hook - JS version (client-side)
[@platform js]
let useQuery =
    (
      type p,
      type r,
      module Q: QueryModule with type params = p and type row = r,
      params: p,
      (),
    ) => {
  let channel = Q.channel(params);
  let paramsHash = Q.paramsHash(params);
  let key = makeKey(~channel, ~paramsHash);

  let cache = getQueryCache();
  let signal = useQuerySignal(~cache, ~key, ~channel);
  let currentResult = signal->Tilia.Core.lift;

  switch (currentResult) {
  | Loading => {
      data: Loading,
      loading: true,
      error: None,
    }
  | Loaded(jsonRows) =>
    try({
      let decodedRows = jsonRows->Js.Array.map(~f=Q.decodeRow);
      {
        data: Loaded(decodedRows),
        loading: false,
        error: None,
      };
    }) {
    | _ =>
      let message = "Failed to decode query result";
      {
        data: Error(message),
        loading: false,
        error: Some(message),
      }
    };
  | Error(msg) => {
      data: Error(msg),
      loading: false,
      error: Some(msg),
    }
  };
};

// Main useQuery hook - Native version (server-side SSR)
[@platform native]
let useQuery =
    (
      type p,
      type r,
      module Q: QueryModule with type params = p and type row = r,
      params: p,
      (),
    ) => {
  let channel = Q.channel(params);
  let paramsHash = Q.paramsHash(params);
  let key = makeKey(~channel, ~paramsHash);

  // Server: Register with QueryRegistry for SSR collection
  let _ =
    QueryRegistry.register_query(
      ~key,
      ~channel,
      ~params,
      ~sql="",
      ~execute=
        db => {
          Lwt.bind(
            Q.execute(db, params),
            (result: Stdlib.result(array(r), string)) => {
            switch (result) {
            | Ok(rows) =>
              let jsonRows =
                rows->Js.Array.map(~f=(row: r) =>
                  row->Q.row_to_json->StoreJson.toSafe
                );
              Lwt.return(Stdlib.Ok(StoreJson.safeListOfArray(jsonRows)));
            | Error(msg) => Lwt.return(Stdlib.Error(msg))
            }
          })
        },
      ~decode=json => Q.decodeRow(StoreJson.ofSafe(json)),
    );

  // Get result from registry if available
  let registry_opt =
    switch (Lwt.get(QueryRegistry.registry_key)) {
    | Some(r) => Some(r)
    | None => QueryRegistry.sync_registry_ref^
    };
  let data =
    switch (registry_opt) {
    | Some(registry) =>
      switch (Hashtbl.find_opt(registry.results, key)) {
      | Some(json) =>
        try(
          {
            let rows_ =
              switch ((json : Yojson.Safe.t)) {
              | `List(rowJsons) =>
                let rowJsons = rowJsons |> Array.of_list;
                rowJsons->Js.Array.map(~f=rowJson =>
                  Q.decodeRow(StoreJson.ofSafe(rowJson))
                );
              | _ => [|Q.decodeRow(StoreJson.ofSafe(json))|]
              };
            Loaded(rows_)
          }
        ) {
        | _ => Error("Failed to decode query result")
        }
      | None => Loading
      }
    | None => Loading
    };

  let loading =
    switch (data) {
    | Loading => true
    | _ => false
    };

  let error =
    switch (data) {
    | Error(msg) => Some(msg)
    | _ => None
    };

  {
    data,
    loading,
    error,
  };
};

// Helper to check if query is loading
[@platform js]
let useIsQueryLoading =
    (
      type p,
      type r,
      module Q: QueryModule with type params = p and type row = r,
      params: p,
    ) => {
  let channel = Q.channel(params);
  let paramsHash = Q.paramsHash(params);
  let key = makeKey(~channel, ~paramsHash);

  let cache = getQueryCache();
  let signal = useQuerySignal(~cache, ~key, ~channel);
  switch (signal->Tilia.Core.lift) {
  | Loading => true
  | Loaded(_)
  | Error(_) => false
  };
};

[@platform native]
let useIsQueryLoading =
    (
      type p,
      type r,
      module Q: QueryModule with type params = p and type row = r,
      params: p,
    ) => {
  let result = useQuery((module Q), params, ());
  result.loading;
};
