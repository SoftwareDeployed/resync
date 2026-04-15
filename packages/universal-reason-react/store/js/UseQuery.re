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

// Main useQuery hook
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

  switch%platform (Runtime.platform) {
  | Client =>
    // Client: Subscribe via QueryCache, decode raw JSON on access
    let cache = getQueryCache();

    // Subscribe to cache updates
    let (signal, unsubscribe) =
      React.useMemo1(
        () => {
          QueryCache.subscribe(
            ~t=cache,
            ~key,
            ~channel,
            ~decodeRow=Q.decodeRow,
            ~updatedAt=0.0,
            (),
          );
        },
        [|key|],
      );

    // Track current result with React state
    let (result, setResult) =
      React.useState(() => {
        let initialSignalValue = Tilia.Core.lift(signal);
        switch (initialSignalValue) {
        | Loading => {data: Loading, loading: true, error: None}
        | Loaded(jsonRows) =>
          let decodedRows = jsonRows->Js.Array.map(~f=Q.decodeRow);
          {data: Loaded(decodedRows), loading: false, error: None};
        | Error(msg) => {data: Error(msg), loading: false, error: Some(msg)};
        };
      });

    // Effect to subscribe to signal changes
    React.useEffect1(
      () => {
        // Set up subscription to signal changes
        let currentResult = Tilia.Core.lift(signal);
        let newResult =
          switch (currentResult) {
          | Loading => {data: Loading, loading: true, error: None}
          | Loaded(jsonRows) =>
            let decodedRows = jsonRows->Js.Array.map(~f=Q.decodeRow);
            {data: Loaded(decodedRows), loading: false, error: None};
          | Error(msg) => {data: Error(msg), loading: false, error: Some(msg)};
          };
        
        // Only update if different
        if (newResult.data != result.data) {
          setResult(_ => newResult);
        };

        // Cleanup subscription on unmount or key change
        Some(unsubscribe);
      },
      [|key|],
    );

    result;

  | Server =>
    // Server: Register with QueryRegistry for SSR collection
    // Note: On native, QueryRegistry stores Yojson.Safe.t but Q.decodeRow
    // expects StoreJson.json (which is Yojson.Basic.t on native).
    // We use Obj.magic for this safe conversion since both are JSON representations.
    let _ =
      QueryRegistry.register_query(
        ~key,
        ~channel,
        ~params,
        ~sql="",
        ~execute=db => {
          Lwt.bind(Q.execute(db, params), (result: Stdlib.result(array(r), string)) => {
            switch (result) {
            | Ok(rows) =>
              // Convert array of rows to Yojson.Safe.t
              // First convert rows to Basic.t, then to string, then parse as Safe.t
              let jsonRows = rows->Js.Array.map(~f=(row: r) => {
                let basicJson = Q.row_to_json(row);
                let jsonStr = StoreJson.stringify(_x => basicJson, ());
                Yojson.Safe.from_string(jsonStr);
              });
              Lwt.return(Stdlib.Ok(`List(jsonRows |> Array.to_list)));
            | Error(msg) => Lwt.return(Stdlib.Error(msg))
            }
          })
        },
        ~decode=json => Q.decodeRow(Obj.magic(json)),
      );

    // Get result from registry if available
    let data =
      switch (Lwt.get(QueryRegistry.registry_key)) {
      | Some(registry) =>
        switch (Hashtbl.find_opt(registry.results, key)) {
        | Some(json) =>
          try(Loaded([|Q.decodeRow(Obj.magic(json))|])) {
          | _ => Loading
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

    {data, loading, error};
  };
};

// Helper to check if query is loading
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

  switch%platform (Runtime.platform) {
  | Client =>
    let cache = getQueryCache();
    switch (QueryCache.getResult(~t=cache, ~key)) {
    | Some(Loading) => true
    | Some(_) => false
    | None => true
    };
  | Server =>
    // On server, queries are executed synchronously in two-pass render
    // After first pass, all queries should be loaded
    let _ = key;
    false
  };
};
