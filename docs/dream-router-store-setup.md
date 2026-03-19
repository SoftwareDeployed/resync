# Dream + Universal Router + Store

This repo now has a working pattern for building a Dream app with:

- `packages/universal-reason-react/router`
- `packages/universal-reason-react/store`
- server-side bootstrap via `getServerState`
- client hydration + realtime updates through the store

The ecommerce demo is the reference implementation:

- routes: `demos/ecommerce/ui/src/Routes.re`
- main store: `demos/ecommerce/ui/src/Store.re`
- persisted cart store: `demos/ecommerce/ui/src/CartStore.re`
- server state + render: `demos/ecommerce/server/src/EntryServer.re`
- Dream wiring: `demos/ecommerce/server/src/server.ml`

This setup is still a prototype. Expect the APIs to keep changing while the router and store authoring model are refined.

This document is AI-generated and should be treated as a draft until it has been reviewed and edited by a human.

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

For an SSR + realtime store, use `StoreBuilder.Runtime.Make`.

```reason
[@deriving json]
type payload = {
  config: config_payload,
  unit: PeriodList.Unit.t,
};

type patch =
  | InventoryUpsert(inventory_patch_data)
  | InventoryDelete(string);

let payloadOfConfig = (config: config): payload => ...;
let configOfPayload = (payload: payload): config => ...;

let makeStore = (~config, ~payload, ~derive=?, ()) => {
  {
    config,
    premise_id:
      StoreBuilder.projected(
        ~derive?,
        ~project,
        ~serverSource=config,
        ~fromStore=store => store.config,
        ~select=projections => projections.premise_id,
        (),
      ),
    unit:
      StoreBuilder.current(
        ~derive?,
        ~client=PeriodList.Unit.value,
        ~server=() => payload.unit,
        (),
      ),
  };
};

let decodePatch =
  StorePatch.compose([
    StorePatch.Pg.decodeAs(
      ~table="inventory",
      ~decodeRow=inventory_patch_data_of_json,
      ~insert=data => InventoryUpsert(data),
      ~update=data => InventoryUpsert(data),
      ~delete=id => InventoryDelete(id),
      (),
    ),
  ]);

let updateOfPatch = patch =>
  switch (patch) {
  | InventoryUpsert(data) => config => ...
  | InventoryDelete(id) => config => ...
  };

module Runtime = StoreBuilder.Runtime.Make({
  type nonrec config = config;
  type nonrec patch = patch;
  type nonrec payload = payload;
  type nonrec store = store;
  type nonrec subscription = subscription;

  let emptyStore = emptyStore;
  let stateElementId = stateElementId;
  let payloadOfConfig = payloadOfConfig;
  let configOfPayload = configOfPayload;
  let makeStore = makeStore;
  let config_of_json = config_of_json;
  let config_to_json = config_to_json;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
  let decodePatch = decodePatch;
  let subscriptionOfConfig = subscriptionOfConfig;
  let encodeSubscription = encodeSubscription;
  let updatedAtOf = updatedAtOf;
  let updateOfPatch = updateOfPatch;
  let eventUrl = eventUrl;
  let baseUrl = baseUrl;
});

include (
  Runtime:
    StoreBuilder.Runtime.Exports
      with type config := config
      and type payload := payload
      and type t := store
);

module Context = Runtime.Context;
```

For a persisted client-side store, use `StoreBuilder.Persisted.Make`.

## 4. Use typed patches, not raw string tuples

Do not leave patch handling at the level of:

```reason
(patch.type_, patch.table_, patch.action)
```

Instead:

- decode transport envelopes into app-specific variants
- convert each patch into an updater function
- let `StoreSync` push those updaters into the Tilia-backed source

That keeps the store API more FRP-like:

- snapshots become `source.set(snapshot)`
- patches become `source.update(updateOfPatch(patch))`

## 5. Sync with `RealtimeClient`

If you use `StoreBuilder.Runtime.Make`, realtime sync is usually configured through the store schema and then wired automatically through `StoreSync`.

The store module provides:

- `subscriptionOfConfig`
- `encodeSubscription`
- `updatedAtOf`
- `decodePatch`
- `updateOfPatch`
- `eventUrl`
- `baseUrl`

That is enough for `StoreSync` to call `RealtimeClient.Socket.subscribe(...)` for you.

At a low level, the flow is:

```reason
RealtimeClient.Socket.subscribe(
  ~source,
  ~subscription=Schema.encodeSubscription(subscription),
  ~updatedAt=Schema.updatedAtOf(config),
  ~decodePatch=Schema.decodePatch,
  ~updateOfPatch=Schema.updateOfPatch,
  ~decodeSnapshot=Schema.config_of_json,
  ~updatedAtOf=Schema.updatedAtOf,
  ~eventUrl=Schema.eventUrl,
  ~baseUrl=Schema.baseUrl,
);
```

The important part is that sync talks to the captured `StoreSource` actions:

- full snapshots call `source.set(snapshot)`
- patches call `source.update(updateOfPatch(patch))`

That keeps realtime updates flowing through the same Tilia-backed source that hydration and local actions use.

For the ecommerce demo, the relevant wiring lives in:

- `packages/universal-reason-react/store/js/RealtimeClient.re`
- `packages/universal-reason-react/store/js/StoreSync.re`
- `demos/ecommerce/ui/src/Store.re`

## 6. Define `getServerState` in the server entry module

`getServerState` should fetch the initial source state for the first SSR request only.

```reason
let getServerState = (context: UniversalRouterDream.serverContext) => {
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let* premise =
    Dream.sql(
      UniversalRouterDream.contextRequest(context),
      Database.Premise.get_route_premise(routeRoot),
    );

  switch (premise) {
  | None => Lwt.return(UniversalRouterDream.NotFound)
  | Some(premise) =>
    let* inventory =
      Dream.sql(
        UniversalRouterDream.contextRequest(context),
        Database.Inventory.get_list(premise.id),
      );
    let config: Config.t = {inventory, premise: Some(premise)};
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
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let serverPath = UniversalRouterDream.contextPath(context);
  let serverSearch = UniversalRouterDream.contextSearch(context);

  let app =
    <UniversalRouter
      router=Routes.router
      routeRoot
      serverPath
      serverSearch
    />;

  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~routeRoot,
      ~path=serverPath,
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

In `server.ml`, keep explicit non-page Dream routes first, then mount the universal app handler.

```reason
Dream.router([
  Dream.get "/_events" realtime_handler,
  Dream.get "/static/**" (Dream.static doc_root),
  Dream.get "/app.js" app_js_handler,
  Dream.get "/style.css" css_handler,
  Dream.get "/" (UniversalRouterDream.handler ~app:EntryServer.app),
  Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app),
]);
```

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

## 11. Recommended project layout

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
  Config.re         shared domain types / codecs
  PeriodList.re     shared period units / codecs
```

## 12. Current prototype direction

This repo is currently optimized for a prototype workflow:

- typed patch variants preferred over stringly typed patch reducers
- `getServerState` preferred over generic page props
- one shared route tree for Dream and the client
- APIs are intentionally still in flux while the overall model is being validated

If you are unsure how to structure a new app, start by copying the ecommerce demo shape and then simplify from there.
