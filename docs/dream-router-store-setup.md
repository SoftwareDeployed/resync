# Dream + Universal Router + Store

This repo now has a working pattern for building a Dream app with:

- `packages/universal-reason-react/router`
- `packages/universal-reason-react/store`
- server-side bootstrap via `getServerState`
- client hydration + realtime updates through the store

The ecommerce demo is the reference implementation:

- routes: `demos/ecommerce/ui/src/Routes.re`
- main store: `demos/ecommerce/ui/src/Store.re`
- local-only cart store: `demos/ecommerce/ui/src/CartStore.re`
- server state + render: `demos/ecommerce/server/src/EntryServer.re`
- Dream wiring: `demos/ecommerce/server/src/server.ml`

This setup is still a prototype. Expect the APIs to keep changing while the router and store authoring model are refined.

This guide demonstrates prototype-ready patterns for universal React applications with Dream. The APIs are not stable and are subject to change.

## Development environment

For local development, set `DB_URL`, `API_BASE_URL`, and the app-specific doc-root env var before starting the Dream server. In the ecommerce demo that variable is `ECOMMERCE_DOC_ROOT`.
Using `.envrc` is recommended.

```bash
export DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
API_BASE_URL="http://localhost:8899" \
ECOMMERCE_DOC_ROOT="./_build/default/demos/ecommerce/ui/src/"
```

The current ecommerce server setup expects `DB_URL` and `ECOMMERCE_DOC_ROOT` to be present.

## Mental model

- `UniversalRouter` owns route matching, layouts, href generation, and document rendering.
- `UniversalRouterDream` owns Dream request integration and SSR entrypoints.
- `StoreBuilder` owns the Tilia-backed store authoring pattern.
- `getServerState` runs once on the initial SSR request and returns the initial source state for the store.
- After hydration, realtime patches update the same Tilia-backed source on the client.

## 1. Add the package dependencies

UI-side Dune libraries usually need:

```lisp
(libraries
 common_js
 universal_reason_react_components_js
 universal_reason_react_router_js
 universal_reason_react_store_js
 reason-react
 melange.js)
```

Server-side Dune libraries usually need:

```lisp
(libraries
 dream
 server-reason-react.react
 server-reason-react.reactDom
 server-reason-react.js
 universal_reason_react_components_native
 universal_reason_react_router_native
 universal_reason_react_store_native)
```

## 2. Define routes once in UI code

Create a route tree in UI code so both the client and Dream can consume the same router.

```reason
let router =
  UniversalRouter.create(
    ~document=UniversalRouter.document(~title="My App", ()),
    ~notFound=(module NotFoundPage),
    [
      UniversalRouter.group(
        ~path="",
        ~layout=(module RootLayout),
        [
          UniversalRouter.index(~id="home", ~page=(module HomePage), ()),
          UniversalRouter.route(
            ~id="item",
            ~path="item/:id",
            ~page=(module ItemPage),
            [],
            (),
          ),
        ],
        (),
      ),
    ],
  );
```

Keep route definitions close to the page and layout components they reference.

## 3. Define a store with `StoreBuilder`

For an SSR + realtime store, use `StoreBuilder.buildSynced`. For a local-only store, use `StoreBuilder.buildLocal`.

Both builders:

- hydrate synchronously from SSR
- reconcile confirmed state from IndexedDB after mount
- use `storeName` plus `scopeKeyOfState` as the browser persistence identity

`Synced.Define` additionally:

- persists an IndexedDB action ledger
- sends typed JSON actions over the websocket
- reconciles with ack, patch, and snapshot frames

```reason
[@deriving json]
type state = Model.t;

type subscription = RealtimeSubscription.t;

type patch = StoreCrud.patch(Model.InventoryItem.t);

type action = Noop;

type store = {
  premise_id: string,
  config: state,
  period_list: array(Model.Pricing.period),
  unit: PeriodList.Unit.t,
};

module StoreDef =
  (val StoreBuilder.buildSynced(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState: Model.SSR.empty,
         reduce: (~state: state, ~action: action) => state,
         makeStore:
           (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
           {
             premise_id:
               StoreBuilder.projected(
                 ~derive?,
                 ~project,
                 ~serverSource=state,
                 ~fromStore=store => store.config,
                 ~select=projections => projections.premise_id,
                 (),
               ),
             config: state,
             period_list:
               StoreBuilder.projected(
                 ~derive?,
                 ~project,
                 ~serverSource=state,
                 ~fromStore=store => store.config,
                 ~select=projections => projections.period_list,
                 (),
               ),
             unit:
               StoreBuilder.current(
                 ~derive?,
                 ~client=PeriodList.Unit.value,
                 ~server=() => PeriodList.Unit.defaultState,
                 (),
               ),
           };
         },
       })
    |> StoreBuilder.withJson(
         ~state_of_json,
         ~state_to_json,
         ~action_of_json: _json => Noop,
         ~action_to_json: _action => StoreJson.parse("{\"kind\":\"noop\"}"),
       )
    |> StoreBuilder.withSync(
         ~storeName="ecommerce.inventory",
         ~scopeKeyOfState=(state: state) =>
           switch (state.premise) {
           | Some(premise) => premise.id
           | None => "default"
           },
         ~timestampOfState=(state: state) =>
           switch (state.premise) {
           | Some(premise) => premise.updated_at->Js.Date.getTime
           | None => 0.0
           },
         ~setTimestamp=(~state: state, ~timestamp: float) =>
           switch (state.premise) {
           | Some(premise) => {
               ...state,
               premise: Some({...premise, updated_at: Js.Date.fromFloat(timestamp)}),
             }
           | None => state
           },
         ~decodePatch=
           StorePatch.compose([
             StoreCrud.decodePatch(
               ~table=RealtimeSchema.table_name("inventory"),
               ~decodeRow=Model.InventoryItem.of_json,
               (),
             ),
           ]),
         ~updateOfPatch=
           StoreCrud.updateOfPatch(
             ~getId=(item: Model.InventoryItem.t) => item.id,
             ~getItems=(state: state) => state.inventory,
             ~setItems=(state: state, items) => {...state, inventory: items},
           ),
         ~transport={
           subscriptionOfState: (state: state) =>
             switch (state.premise) {
             | Some(premise) => Some(RealtimeSubscription.premise(premise.id))
             | None => None
             },
           encodeSubscription: RealtimeSubscription.encode,
           eventUrl: Constants.event_url,
           baseUrl: Constants.base_url,
         },
         ~stateElementId=Some("initial-store"),
         (),
       )
  ));

include (
  StoreDef:
    StoreBuilder.Runtime.Exports
      with type state := state
      and type action := action
      and type t := store
);

type t = store;

module Context = StoreDef.Context;
```

For a local-only client store such as the ecommerce cart, use `StoreBuilder.buildLocal` with the same pipeline pattern (ending in `withLocalPersistence` instead of `withSync`). It still uses IndexedDB for confirmed state and syncs newer confirmed snapshots across tabs with `BroadcastChannel`, but it does not create an action ledger or open a websocket.

### 3a. Minimal end-to-end todo flow

The todo demo is the shortest path from store authoring through server rendering and client hydration.

**Server entry (`demos/todo/server/src/EntryServer.re`)**

```reason
let getServerState = (_context: UniversalRouterDream.serverContext(TodoStore.t)) => {
  let state: TodoStore.state = {
    todos: [|
      {id: "1", text: "Learn ReasonML", completed: false},
      {id: "2", text: "Build an app", completed: false},
      {id: "3", text: "Deploy to production", completed: false},
    |],
    updated_at: 0.0,
  };

  let store = TodoStore.createStore(state);
  Lwt.return(UniversalRouterDream.State(store));
};

let render = (~context, ~serverState: TodoStore.t, ()) => {
  let {UniversalRouterDream.basePath, UniversalRouterDream.pathname: serverPathname, UniversalRouterDream.search: serverSearch} = context;
  let serializedState = TodoStore.serializeState(serverState.state);

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
      ~state=serverState,
      (),
    );

  <TodoStore.Context.Provider value=serverState>
    document
  </TodoStore.Context.Provider>;
};
```

**Client entry (`demos/todo/ui/src/Index.re`)**

```reason
let store = TodoStore.hydrateStore();

let app =
  React.createElement(
    TodoStore.Context.Provider.make,
    {
      "value": store,
      "children": <UniversalRouter router=Routes.router state=store />,
    },
  );

ReactDOM.Client.hydrateRoot(root, app) |. ignore;
```

**Page component (`demos/todo/ui/src/HomePage.re`)**

```reason
let store = TodoStore.Context.useStore();

let handleSubmit = event => {
  preventDefault(event);
  let text = String.trim(newTodoText);
  if (text != "") {
    TodoStore.addTodo(store, text);
    setNewTodoText(_ => "");
  };
};
```

For the full store authoring code that feeds this flow, see `docs/universal-reason-react.store.md` and `demos/todo/ui/src/TodoStore.re`.

## 4. Use typed patches with `StoreCrud`

Use `StoreCrud.patch('row)` as your patch type and `StoreCrud.decodePatch`/`StoreCrud.updateOfPatch` for standard CRUD tables:

```reason
type patch = StoreCrud.patch(MyItem.t);

let decodePatch =
  StorePatch.compose([
    StoreCrud.decodePatch(
      ~table=RealtimeSchema.table_name("items"),
      ~decodeRow=MyItem.of_json,
      (),
    ),
  ]);

let updateOfPatch = StoreCrud.updateOfPatch(
  ~getId=(item: MyItem.t) => item.id,
  ~getItems=(config: config) => config.items,
  ~setItems=(config: config, items) => {...config, items},
);
```

For multi-table patches with different config fields, use a wrapped variant:

```reason
type patch =
  | ItemsPatch(StoreCrud.patch(Item.t))
  | UsersPatch(StoreCrud.patch(User.t));
```

## 5. Sync with `RealtimeClient`

If you use `StoreBuilder.buildSynced`, realtime sync is configured through the store schema and wired automatically by the runtime builder.

The store module provides:

- `subscriptionOfState` (in `transport`)
- `encodeSubscription` (in `transport`)
- `timestampOfState`
- `decodePatch` (in `strategy`)
- `updateOfPatch` (in `strategy`)
- `eventUrl` (in `transport`)
- `baseUrl` (in `transport`)
- `action_of_json`
- `action_to_json`

That is enough for `StoreBuilder.buildSynced` to:

- subscribe over the active websocket
- persist confirmed snapshots and queued actions to IndexedDB
- send `{type: "mutation", actionId, action}` frames
- handle `ack`, `patch`, and `snapshot` frames

If your app does not run on the default ecommerce port, set `baseUrl` explicitly from app constants rather than relying on `RealtimeClient.Socket.defaultBaseUrl`.

At a low level, the flow is:

The important part is that sync talks to the captured `StoreSource` actions and a persisted confirmed snapshot:

- full snapshots call `source.set(snapshot)`
- patches call `source.update(updateOfPatch(patch))`
- optimistic actions are replayed from the IndexedDB action ledger until confirmed

That keeps realtime updates flowing through the same Tilia-backed source that hydration and local actions use.

Typed runtime actions use the same active websocket connection. The client-side write path is:

```reason
dispatch(AddTodo({id, list_id, text}));
```

That reduces optimistically, writes the queued action to IndexedDB, sends a JSON mutation frame over the socket, and waits for an `ack`, patch, or snapshot to advance confirmed state.

For the ecommerce demo, the relevant wiring lives in:

- `packages/universal-reason-react/store/js/RealtimeClient.re`
- `packages/universal-reason-react/store/js/StoreOffline.re`
- `packages/universal-reason-react/store/js/StoreIndexedDB.re`
- `demos/ecommerce/ui/src/Store.re`

## 6. Define `getServerState` in the server entry module

`getServerState` should fetch the initial source state for the first SSR request only.

```reason
let getServerState = (context: UniversalRouterDream.serverContext) => {
  let {UniversalRouterDream.basePath, UniversalRouterDream.request} = context;
  let* premise =
    Dream.sql(
      request,
      Database.Premise.get_route_premise(basePath),
    );

  switch (premise) {
  | None => Lwt.return(UniversalRouterDream.NotFound)
  | Some(premise) =>
    let* inventory =
        Dream.sql(request, Database.Inventory.get_list(premise.id));
    let config: Model.t = {inventory, premise: Some(premise)};
    Lwt.return(UniversalRouterDream.State(config))
  };
};
```

This is not a client navigation loader. It only seeds the initial store state.

## 7. Bundle the server app with `UniversalRouterDream.app`

Create one server app value that knows:

- which router to use
- how to fetch initial server state
- how to render the app

```reason
let render = (~context, ~serverState, ()) => {
  let store = Store.createStore(serverState);
  let serializedState = Store.serializeState(serverState);
  let {UniversalRouterDream.basePath, UniversalRouterDream.pathname: serverPathname, UniversalRouterDream.search: serverSearch} = context;

  let app =
    <UniversalRouter
      router=Routes.router
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
      (),
    );

  <Store.Context.Provider value=store>
    document
  </Store.Context.Provider>;
};

let app =
  UniversalRouterDream.app(
    ~router=Routes.router,
    ~getServerState,
    ~render,
    (),
  );
```

## 8. Mount the app in Dream

In `server.ml`, use `Server_builder` to collapse the env-var parsing, adapter startup, and `Dream.run` boilerplate. Keep explicit non-page Dream routes first, then mount the universal app handler.

```ocaml
let () =
  let builder =
    Server_builder.make
      ~doc_root_var:"ECOMMERCE_DOC_ROOT"
      ~db_url_var:"DB_URL"
      ~default_interface:"127.0.0.1"
      ~default_port:8899
      ()
  in
  let doc_root = Server_builder.doc_root builder in
  let db_uri = Option.get (Server_builder.db_uri builder) in
  let adapter =
    Adapter.pack
      (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
      (Pgnotify_adapter.create ~db_uri ())
  in
  builder
  |> Server_builder.with_packed_adapter adapter
  |> Server_builder.with_middleware
       ~resolve_subscription
       ~load_snapshot:get_config_json
  |> Server_builder.with_routes [
    Dream.get "/static/**" (Dream.static doc_root);
    Dream.get "/app.js" (fun req ->
      Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req ->
      Dream.from_filesystem doc_root "Index.re.css" req);
    Dream.get "/" (UniversalRouterDream.handler ~app:EntryServer.app);
    Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
  ]
  |> Server_builder.run
```

If you are using PostgreSQL-backed realtime tables, make sure your setup applies the generated `realtime.sql` triggers before expecting websocket patches to arrive.

## 9. Hydrate on the client

The client entrypoint should hydrate stores first, then render the router.

```reason
let store = Store.hydrateStore();
let cartStore = CartStore.hydrateStore();

ReactDOM.hydrateRoot(
  root,
  <Store.Context.Provider value=store>
    <CartStore.Context.Provider value=cartStore>
      <UniversalRouter router=Routes.router />
    </CartStore.Context.Provider>
  </Store.Context.Provider>,
);
```

## 10. Important conventions

- Normalize payloads at the hydration boundary, not inside random components.
- Keep Tilia hidden behind `universal-reason-react/store`; do not mix ad hoc `Tilia.Core.*` calls through app code unless you are extending the store package itself.
- Treat the store source state as authoritative; projections should be derived from it.
- Make server-side fallback branches lazy when defining derived values, so client construction does not accidentally execute server-only projection work.
- Encode units and other enum-like values as stable strings in JSON.
- Prefer typed patch variants plus `decodePatch`/`updateOfPatch` over stringly typed patch reducers.

## 11. Development watch workaround (UI + server)

The ecommerce demo shares compiled UI artifacts with the server. To keep the server
restarting when a client-side shared file changes, this repository uses a generated
`.build_stamp` file as a lightweight trigger.

### .build_stamp in dune

- `demos/ecommerce/ui/src/dune` builds `Index.re.js` and writes `demos/ecommerce/ui/src/.build_stamp`.
- `demos/ecommerce/server/src/dune` copies that `.build_stamp` into the server
  build context via `copy_files`.
- `package.json` defines `dev:watch`, which watches
  `./_build/default/demos/ecommerce/ui/src/.build_stamp` and restarts
  `./_build/default/demos/ecommerce/server/src/server.exe` when the stamp changes.

This gives practical hot-reload behavior for cases where Dream server rendering uses
source files that are also part of the client build graph.

```bash
pnpm run dune:watch    # keeps client+server artifacts rebuilding
pnpm run dev:watch     # restarts the server when .build_stamp updates
```

> If you add more client files used by the server, ensure they remain in the same
> dependency graph that causes `.build_stamp` to refresh after a rebuild.

## 12. Recommended project layout

```text
ui/src/
  Index.re          client hydration entry
  Routes.re         shared route tree
  Store.re          main SSR + realtime store
  CartStore.re      persisted client store

server/src/
  EntryServer.re    getServerState + render + app
  server.ml         Dream setup + route mounting

shared/js/
  Model.re          shared domain types / codecs
  PeriodList.re     shared period units / codecs
```

## 13. Current prototype direction

This repo is currently optimized for a prototype workflow:

- typed patch variants preferred over stringly typed patch reducers
- `getServerState` preferred over generic page props
- one shared route tree for Dream and the client
- APIs are intentionally still in flux while the overall model is being validated

If you are unsure how to structure a new app, start by copying the ecommerce demo shape and then simplify from there.
