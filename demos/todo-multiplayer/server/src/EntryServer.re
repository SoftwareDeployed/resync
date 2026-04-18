open Lwt.Syntax;

let isHexDigit = (char: char) =>
  switch (char) {
  | '0' .. '9'
  | 'a' .. 'f'
  | 'A' .. 'F' => true
  | _ => false
  };

let isUuid = (value: string) => {
  let length = String.length(value);
  if (Int.equal(length, 36)) {
    let rec loop = index =>
      if (Int.equal(index, length)) {
        true;
      } else {
        let char = String.get(value, index);
        if (
          Int.equal(index, 8)
          || Int.equal(index, 13)
          || Int.equal(index, 18)
          || Int.equal(index, 23)
        ) {
          Char.equal(char, '-') && loop(index + 1);
        } else {
          isHexDigit(char) && loop(index + 1);
        };
      };
    loop(0);
  } else {
    false;
  };
};

// Use the serverState type from Routes
module RoutesServerState = {
  type t = Routes.serverState;
};

let getServerState = (context: UniversalRouterDream.serverContext(Routes.serverState)) => {
  let UniversalRouterDream.{basePath, request, params} = context;
  // Only accept root basePath to ensure correct route matching
  if (basePath != "/") {
    Lwt.return(UniversalRouterDream.NotFound);
  } else {
    // Extract listId from params (set by router when basePath="/")
    switch (UniversalRouter.Params.find("listId", params)) {
    | None => Lwt.return(UniversalRouterDream.NotFound)
    | Some(listId) =>
      if (!isUuid(listId)) {
        Lwt.return(UniversalRouterDream.NotFound);
      } else {
        // Create store with emptyState (queries will populate it)
        let emptyStore = TodoStore.createStore(TodoStore.emptyState);

        // Pre-render pass: collect queries using QueryRegistry
        let serializedState = TodoStore.serializeState(emptyStore.state);
        // Create a dummy serverState for pre-render (queries not yet collected)
        let prerenderServerState: Routes.serverState = {store: emptyStore, serializedQueries: ""};
        // Pre-render with basePath="/" so router matches :listId correctly
        let prerenderApp =
          <UniversalRouter
            router=Routes.router
            state=prerenderServerState
            basePath="/"
            serverPathname={context.pathname}
            serverSearch={context.search}
          />;
        let prerenderDocument =
          UniversalRouter.renderDocument(
            ~router=Routes.router,
            ~children=prerenderApp,
            ~basePath="/",
            ~pathname={context.pathname},
            ~search={context.search},
            ~serializedState,
            ~state=prerenderServerState,
            (),
          );
        let prerenderElement =
          <TodoStore.Context.Provider value=emptyStore>
            prerenderDocument
          </TodoStore.Context.Provider>;

        // Run pre-render within QueryRegistry to collect and execute queries
        let* (store, serializedQueries) =
          Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) => {
            QueryRegistry.with_registry(
              ~db=(module Db),
              ~f=() => {
                // First pass: render to collect queries
                let _html = ReactDOM.renderToString(prerenderElement);
                // Execute all registered queries
                let* () = QueryRegistry.execute_queries();
                // Apply query results to state
                let snapshot = QueryRegistry.get_results();
                let updatedState = snapshot.queries->Js.Array.reduce(
                  ~f=(state: TodoStore.state, key: QueryRegistry.query_key) => {
                    switch (snapshot.results->Js.Dict.get(key)) {
                    | None => state
                    | Some(jsonRows) =>
                      // Extract channel from key (format: "channel:paramsHash")
                      let channel = switch (Js.String.split(~limit=2, key, ~sep=":")) {
                      | [|ch, _|] => ch
                      | [|ch|] => ch
                      | _ => ""
                      };
                      // Parse rows: jsonRows is a Yojson list of row objects
                      let rows = switch (jsonRows) {
                      | `List(items) => items->Array.of_list
                      | _ => [|jsonRows|]
                      };
                      TodoStore.applyQueryResult(~state, ~channel, ~rows);
                    };
                  },
                  ~init=TodoStore.emptyState,
                );
                // Create store with query-populated state
                let store = TodoStore.createStore(updatedState);
                // Serialize query cache for client hydration
                let serialized = QueryRegistry.serialize_for_cache();
                Lwt.return((store, serialized));
              },
              (),
            );
          });

        let serverState: Routes.serverState = {store, serializedQueries};
        Lwt.return(UniversalRouterDream.State(serverState));
      };
    };
  };
};

let render = (~context, ~serverState: Routes.serverState, ()) => {
  let store = serverState.store;
  let serializedQueries = serverState.serializedQueries;
  let serializedState = TodoStore.serializeState(store.state);
  let UniversalRouterDream.{
    basePath,
    pathname: serverPathname,
    search: serverSearch,
  } = context;

  let app =
    <UniversalRouter
      router=Routes.router
      state=serverState
      basePath
      serverPathname
      serverSearch
    />;
  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~basePath,
      ~pathname=serverPathname,
      ~search=serverSearch,
      ~serializedState,
      ~serializedQueries,
      ~state=serverState,
      (),
    );
  QueryRegistry.setup_registry_from_json(~jsonStr=serializedQueries);
  <TodoStore.Context.Provider value=store>
    document
  </TodoStore.Context.Provider>;
};

let app =
  UniversalRouterDream.app(
    ~router=Routes.router,
    ~getServerState,
    ~render,
    (),
  );
