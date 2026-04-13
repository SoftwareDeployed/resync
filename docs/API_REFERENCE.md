# API Reference

> **API Stability**: APIs are **not stable** and are **subject to change**.

Complete API reference for Universal Reason React packages.

## Universal Router

### Types

#### `router`

```reason
type router;
```

The router instance containing all route configurations.

#### `routeConfig`

```reason
type routeConfig;
```

Configuration for a single route or route group.

#### `document`

```reason
type document = {
  title: string,
  meta: array(metaTag),
  links: array(linkTag),
  scripts: array(scriptTag),
};
```

Document configuration for SSR.

#### `serverContext`

```reason
type serverContext;
```

Context passed during server-side rendering.

### Functions

#### `UniversalRouter.create`

```reason
let create: (
  ~document: document,
  ~notFound: module(Page),
  list(routeConfig),
) => router;
```

Creates a new router instance.

**Parameters:**
- `~document`: Document configuration
- `~notFound`: Module for 404 page
- `list(routeConfig)`: List of route definitions

**Example:**
```reason
let router =
  UniversalRouter.create(
    ~document=UniversalRouter.document(~title="My App", ()),
    ~notFound=(module NotFoundPage),
    [
      UniversalRouter.index(~id="home", ~page=(module HomePage), ()),
    ],
  );
```

#### `UniversalRouter.index`

```reason
let index: (
  ~id: string,
  ~page: module(Page),
  unit,
) => routeConfig;
```

Creates an index route (e.g., `/`).

**Parameters:**
- `~id`: Unique route identifier
- `~page`: Page component module

#### `UniversalRouter.route`

```reason
let route: (
  ~id: string,
  ~path: string,
  ~page: module(Page),
  ~resolveTitle: titleResolver=?,
  ~resolveTitleWithState: titleResolverWithState('state)=?,
  ~resolveHeadTags: headTagsResolver=?,
  ~resolveHeadTagsWithState: headTagsResolverWithState('state)=?,
  list(routeConfig('state)),
  unit,
) => routeConfig('state);
```

Creates a route with the given path pattern.

**Parameters:**
- `~id`: Unique route identifier
- `~path`: URL path pattern (e.g., `"product/:id"`)
- `~page`: Page component module
- `list(routeConfig)`: Nested routes

#### `UniversalRouter.group`

```reason
let group: (
  ~path: string,
  ~layout: module(Layout),
  list(routeConfig),
  unit,
) => routeConfig;
```

Groups routes under a common path and layout.

**Parameters:**
- `~path`: URL path prefix
- `~layout`: Layout component module
- `list(routeConfig)`: Child routes

#### `UniversalRouter.document`

```reason
let document: (
  ~title: string,
  ~meta: array(metaTag)=?,
  ~links: array(linkTag)=?,
  ~scripts: array(scriptTag)=?,
  unit,
) => document;
```

Creates document configuration.

### Hooks

#### `useRouter`

```reason
type routerApi = {
  push: string => unit,
  replace: string => unit,
  pushRoute: (~id: string, ~params: Params.t=?, ~searchParams: SearchParams.t=?, unit) => unit,
  replaceRoute: (~id: string, ~params: Params.t=?, ~searchParams: SearchParams.t=?, unit) => unit,
};

let useRouter: unit => routerApi;
```

Hook for programmatic navigation.

**Example:**
```reason
let router = UniversalRouter.useRouter();
router.push("/dashboard");
```

#### `usePathname`

```reason
let usePathname: unit => string;
```

Hook to access the current app pathname.

**Example:**
```reason
let pathname = UniversalRouter.usePathname();
```

#### `useSearchParams`

```reason
let useSearchParams: unit => UniversalRouter.SearchParams.t;
```

Hook to access the current search params.

**Example:**
```reason
let searchParams = UniversalRouter.useSearchParams();
let q = UniversalRouter.SearchParams.get("q", searchParams);
```

#### `useSearch`

```reason
let useSearch: unit => string;
```

Hook to access the raw search string.

#### `useParams`

```reason
let useParams: unit => UniversalRouter.Params.t;
```

Hook to access route params.

**Example:**
```reason
let params = UniversalRouter.useParams();
let id = UniversalRouter.Params.get("id", params);
```

### Server Functions

#### `UniversalRouterDream.app`

```reason
let app: (
  ~router: router,
  ~getServerState: serverContext => Lwt.t(serverStateResult),
  ~render: (~context: serverContext, ~serverState: 'a, unit) => React.element,
  unit,
) => app;
```

Creates a Dream-compatible app configuration.

#### `UniversalRouterDream.handler`

```reason
let handler: (~app: app) => Dream.handler;
```

Converts app to Dream request handler.

#### `UniversalRouterDream.serverContext`

```reason
type serverContext('state) = {
  request: Dream.request,
  basePath: string,
  pathname: string,
  search: string,
  searchParams: UniversalRouter.SearchParams.t,
  params: UniversalRouter.Params.t,
  matchResult: UniversalRouter.matchResult('state),
};
```

`serverContext` is intentionally a concrete record so server handlers can access fields directly with qualified record labels.

```reason
let {UniversalRouterDream.basePath, UniversalRouterDream.request} = context;
```

## Universal Store

### Types

#### `store`

```reason
type store('config) = {
  source: Tilia.source('config),
  projections: projections,
};
```

The store type wrapping Tilia source.

#### `patch`

```reason
type patch('a);
```

Opaque patch type for state updates.

#### `subscription`

```reason
type subscription('a);
```

Opaque subscription type for real-time sync.

#### `listener_id`

```reason
type listener_id;
```

Opaque listener token returned from `Events.listen`.

#### `store_event`

```reason
type store_event('action) =
  | Open
  | Close
  | Reconnect
  | ActionAcked({actionId: string, action: option('action)})
  | ActionFailed({actionId: string, action: option('action), message: string})
  | ConnectionError(string)
  | CustomEvent(StoreJson.json)
  | MediaEvent(StoreJson.json);
```

Typed store runtime event surface. This is intentionally a **store runtime** event model, not a raw websocket frame model. Snapshot and patch transport frames remain internal so reducers stay pure and observer code hooks into store outcomes instead of socket plumbing.

### StoreBuilder.buildLocal

Terminal builder for creating local-only runtime stores via the pipeline API. Local stores persist confirmed snapshots to IndexedDB and propagate newer confirmed state across tabs with `BroadcastChannel`.

**Pipeline:**
1. `StoreBuilder.make()`
2. `StoreBuilder.withSchema({emptyState, reduce, makeStore})`
3. `StoreBuilder.withGuardTree(~guardTree)` *(optional)*
4. `StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)`
5. `StoreBuilder.withLocalPersistence(~storeName, ~scopeKeyOfState, ~timestampOfState, ~stateElementId, ())`

**Example:**
```reason
module StoreDef =
  (val StoreBuilder.buildLocal(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState,
         reduce,
         makeStore: (~state: state, ~derive: Tilia.Core.deriver(store)=?, ()) => {
           state:
             StoreBuilder.current(
               ~derive?,
               ~client=state,
               ~server=() => state,
               (),
             ),
           item_count:
             StoreBuilder.derived(
               ~derive?,
               ~client=store => itemCount(store.state),
               ~server=() => itemCount(state),
               (),
             ),
         },
       })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withLocalPersistence(
         ~storeName="ecommerce.cart",
         ~scopeKeyOfState=_state => "default",
         ~timestampOfState=state => state.updated_at,
         ~stateElementId=Some("cart-store"),
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

**Exports:**
- `type t`: The store type
- `type state`: Source state type
- `type action`: Action type
- `let createStore: state => t`: Create new store
- `let hydrateStore: unit => t`: Hydrate from SSR
- `let serializeState: state => string`: Serialize for SSR
- `let serializeSnapshot: state => string`: Serialize for snapshot
- `let dispatch: action => unit`: Dispatch a typed action
- `module Context`: React context for store access
- `module Events`: Event listener module

### StoreBuilder.buildSynced

Terminal builder for creating custom synced runtime stores via the pipeline API. Synced stores persist confirmed snapshots plus an IndexedDB action ledger, send typed JSON actions over websocket, and propagate optimistic actions plus confirmed snapshots across tabs.

**Pipeline:**
1. `StoreBuilder.make()`
2. `StoreBuilder.withSchema({emptyState, reduce, makeStore})`
3. `StoreBuilder.withGuardTree(~guardTree)` *(optional)*
4. `StoreBuilder.withJson(...)`
5. `StoreBuilder.withSync(~transport, ~decodePatch, ~updateOfPatch, ~storeName, ~scopeKeyOfState, ~timestampOfState, ~setTimestamp, ~stateElementId, ())`

**Transport Configuration:**
```reason
type transportConfig('state, 'subscription) = {
  subscriptionOfState: 'state => option('subscription),
  encodeSubscription: 'subscription => string,
  eventUrl: string,
  baseUrl: string,
};
```

**Hooks Configuration:**
```reason
type hooks('action) = {
  onActionError: option(string => unit),
  onActionAck: option((~dispatch: 'action => unit, ~action: 'action, ~actionId: string) => unit),
  onCustom: option(StoreJson.json => unit),
  onMedia: option(StoreJson.json => unit),
  onError: option((~dispatch: 'action => unit) => string => unit),
  onOpen: option((~dispatch: 'action => unit) => unit),
  onConnectionHandle: option(RealtimeClient.Socket.connection_handle => unit),
};
```

**Example:**
```reason
module StoreDef =
  (val StoreBuilder.buildSynced(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState,
         reduce,
         makeStore: (~state: state, ~derive: Tilia.Core.deriver(store)=?, ()) => {
           room_id:
             switch (state.room) {
             | Some(room) => room.id
             | None => ""
             },
           state,
           peers_count:
             StoreBuilder.derived(
               ~derive?,
               ~client=store =>
                 switch (store.state.room) {
                 | Some(room) => Array.length(room.peers)
                 | None => 0
                 },
               ~server=() =>
                 switch (state.room) {
                 | Some(room) => Array.length(room.peers)
                 | None => 0
                 },
               (),
             ),
         },
       })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withSync(
         ~storeName="video-chat",
         ~scopeKeyOfState=state => state.client_id,
         ~timestampOfState=state => state.updated_at,
         ~setTimestamp,
         ~decodePatch=json =>
           switch (action_of_json(json)) {
           | JoinRoom(_) | LeaveRoom(_) | ToggleVideo(_) | ToggleAudio(_)
           | ResetJoinStatus | JoinRoomAcknowledged => None
           | patch => Some(patch)
           },
         ~updateOfPatch=(patch, state) => reduce(~state, ~action=patch),
         ~transport={
           subscriptionOfState: state => Some(state.client_id),
           encodeSubscription: sub => sub,
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

### StoreBuilder.buildCrud

Terminal builder for creating CRUD synced runtime stores via the pipeline API. `withSyncCrud` pre-wires the patch decoding for standard table-backed stores.

**Pipeline:**
1. `StoreBuilder.make()`
2. `StoreBuilder.withSchema({emptyState, reduce, makeStore})`
3. `StoreBuilder.withGuardTree(~guardTree)` *(optional)*
4. `StoreBuilder.withJson(...)`
5. `StoreBuilder.withSyncCrud(~transport, ~table, ~decodeRow, ~getId, ~getItems, ~setItems, ~storeName, ~scopeKeyOfState, ~timestampOfState, ~setTimestamp, ~stateElementId, ())`

**Example:**
```reason
module StoreDef =
  (val StoreBuilder.buildCrud(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState,
         reduce,
         makeStore: (~state: state, ~derive: Tilia.Core.deriver(store)=?, ()) => {
           list_id:
             switch (state.list) {
             | Some(list) => list.id
             | None => ""
             },
           state,
           completed_count:
             StoreBuilder.Crud.filteredCount(
               ~derive?,
               ~getItems=(store: store) => store.state.todos,
               ~predicate=(item: Model.Todo.t) => item.completed,
               (),
             ),
           total_count:
             StoreBuilder.Crud.totalCount(
               ~derive?,
               ~getItems=(store: store) => store.state.todos,
               (),
             ),
         },
       })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withSyncCrud(
         ~storeName="todo-multiplayer",
         ~scopeKeyOfState,
         ~timestampOfState,
           ~setTimestamp,
           ~transport={
             subscriptionOfState: state =>
               switch (state.list) {
               | Some(list) => Some(RealtimeSubscription.list(list.id))
               | None => None
               },
             encodeSubscription: RealtimeSubscription.encode,
             eventUrl: Constants.event_url,
             baseUrl: Constants.base_url,
           },
           ~table=RealtimeSchema.table_name("todos"),
           ~decodeRow=Model.Todo.of_json,
           ~getId=(todo: Model.Todo.t) => todo.id,
           ~getItems=(state: state) => state.todos,
           ~setItems=(state: state, items) => {...state, todos: items},
           ~stateElementId=Some("initial-store"),
           (),
         );
  });

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

### Sync Pipeline Helpers

In the pipeline API, patch handling is provided directly to `withSync` or `withSyncCrud`.

#### `withSync` patch parameters

```reason
~decodePatch: StoreJson.json => option('patch),
~updateOfPatch: ('patch, 'state) => 'state,
```

Use these when you need full control over patch decoding and application. The `decodePatch` function returns `None` for actions that should be handled by the reducer instead of as patches.

#### `withSyncCrud` CRUD parameters

```reason
~table: string,
~decodeRow: StoreJson.json => 'row,
~getId: 'row => string,
~getItems: 'state => array('row),
~setItems: ('state, array('row)) => 'state,
```

Use `withSyncCrud` for standard table-backed stores; it pre-wires `decodePatch` and `updateOfPatch` automatically.

### StoreBuilder.GuardTree

Declarative action validation trees that branch on state predicates.

```reason
type t('state, 'action);
```

#### Constructors

- `WhenTrue('state => bool, t('state, 'action), t('state, 'action))` — branch on a state predicate
- `DenyIf('action => bool, string)` — deny matching actions with a reason
- `AllowIf('action => bool)` — allow matching actions
- `AcceptAll` — allow all actions
- `Pass` — fall through (resolved as `Allow` in `resolve`)

#### `GuardTree.whenTrue`

```reason
let whenTrue:
  (
    ~condition: 'state => bool,
    ~then_: t('state, 'action),
    ~else_: t('state, 'action)=?,
    unit
  )
  => t('state, 'action);
```

#### `GuardTree.denyIf`

```reason
let denyIf:
  (~predicate: 'action => bool, ~reason: string, unit) => t('state, 'action);
```

#### `GuardTree.allowIf`

```reason
let allowIf:
  (~predicate: 'action => bool, unit) => t('state, 'action);
```

#### `GuardTree.resolve`

```reason
let resolve:
  (~state: 'state, ~action: 'action, t('state, 'action)) => StoreRuntimeTypes.guardResult;
```

**Example:**
```reason
let guardTree =
  StoreBuilder.GuardTree.whenTrue(
    ~condition=(state) =>
      switch (state.current_thread_id) {
      | Some(_) => true
      | None => false
      },
    ~then_=StoreBuilder.GuardTree.acceptAll,
    ~else_=
      StoreBuilder.GuardTree.denyIf(
        ~predicate=(action) =>
          switch (action) {
          | SendPrompt(_) | DeleteThread(_) | SelectThread(_) => true
          | _ => false
          },
        ~reason="No active thread",
        (),
      ),
    (),
  );
```

Wire into the pipeline with `withGuardTree`:
```reason
StoreBuilder.make()
|> StoreBuilder.withSchema({...})
|> StoreBuilder.withGuardTree(~guardTree)
|> StoreBuilder.withJson(...)
```

### StoreBuilder.Selectors

Convenience functions for common selector patterns that reduce boilerplate for projection authoring.

#### `Selectors.passthrough`

```reason
let passthrough: (~derive: option(Tilia.Core.deriver('store))=?, ~value: 'a, unit) => 'a;
```

Returns the value as-is on both client and server. Useful for simple state fields.

#### `Selectors.clientOnly`

```reason
let clientOnly:
  (~derive: option(Tilia.Core.deriver('store))=?, ~client: 'a, ~serverDefault: 'a, unit)
  => 'a;
```

Uses the client value on the client and a default on the server. Useful for client-specific state like UI toggle flags.

#### `Selectors.arrayLength`

```reason
let arrayLength:
  (~derive: option(Tilia.Core.deriver('store))=?, ~getArray: 'store => array('a), unit)
  => int;
```

Derived count from array length. Returns 0 on the server.

#### `Selectors.filteredCount`

```reason
let filteredCount:
  (
    ~derive: option(Tilia.Core.deriver('store))=?,
    ~getArray: 'store => array('a),
    ~predicate: 'a => bool,
    unit
  )
  => int;
```

Derived count of items matching a predicate. Returns 0 on the server.

#### `Selectors.field`

```reason
let field:
  (
    ~derive: option(Tilia.Core.deriver('store))=?,
    ~serverSource: 'state,
    ~fromStore: 'store => 'state,
    ~getField: 'state => 'field,
    unit
  )
  => 'field;
```

Extracts a nested field with proper SSR/hydration handling.

#### `Selectors.computed`

```reason
let computed:
  (
    ~derive: option(Tilia.Core.deriver('store))=?,
    ~serverSource: 'state,
    ~fromStore: 'store => 'state,
    ~compute: 'state => 'result,
    unit
  )
  => 'result;
```

Applies a transform function to state.

### StoreBuilder.Crud

Convenience helpers layered on top of `DefineCrud` for common CRUD patterns.

#### `Synced.Crud.totalCount`

```reason
let totalCount:
  (~derive: option(Tilia.Core.deriver('store))=?, ~getItems: 'store => array('row), unit)
  => int;
```

Total items count derived selector. Returns 0 on the server.

**Example:**
```reason
total_count:
  StoreBuilder.Crud.totalCount(
    ~derive?,
    ~getItems=(store: store) => store.state.todos,
    (),
  ),
```

#### `Synced.Crud.filteredCount`

```reason
let filteredCount:
  (
    ~derive: option(Tilia.Core.deriver('store))=?,
    ~getItems: 'store => array('row),
    ~predicate: 'row => bool,
    unit
  )
  => int;
```

Filtered items count derived selector. Returns 0 on the server.

**Example:**
```reason
completed_count:
  StoreBuilder.Crud.filteredCount(
    ~derive?,
    ~getItems=(store: store) => store.state.todos,
    ~predicate=(item: Model.Todo.t) => item.completed,
    (),
  ),
```

### StoreBuilder.Bootstrap

Provider and hydration helpers that reduce boilerplate for store initialization.

#### `Bootstrap.withHydratedProvider`

```reason
let withHydratedProvider:
  (~hydrateStore: unit => 'store, ~provider: 'provider, ~children: React.element)
  => {store: 'store, element: React.element};
```

Hydrates a store and wraps children with its provider. Returns a record with both the store and the wrapped element.

**Example:**
```reason
let result =
  StoreBuilder.Bootstrap.withHydratedProvider(
    ~hydrateStore=TodoStore.hydrateStore,
    ~provider=TodoStore.Context.Provider.make,
    ~children=<App />,
  );
ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
```

#### `Bootstrap.withHydratedProviders`

```reason
let withHydratedProviders:
  (
    ~stores: array((string, unit => 'store, 'provider)),
    ~children: React.element
  )
  => {stores: array('store), element: React.element};
```

Hydrates multiple stores and wraps children with nested providers. Stores are hydrated in order, with the first store as the outermost provider.

**Example:**
```reason
let result = StoreBuilder.Bootstrap.withHydratedProviders(
  ~stores=[|
    ("store", Store.hydrateStore, Store.Context.Provider.make),
    ("cart", CartStore.hydrateStore, CartStore.Context.Provider.make),
  |],
  ~children=<App />,
);
ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
```

#### `Bootstrap.withCreatedProvider`

```reason
let withCreatedProvider:
  (
    ~createStore: 'state => 'store,
    ~provider: 'provider,
    ~initialState: 'state,
    ~children: React.element
  )
  => {store: 'store, element: React.element};
```

Creates a store on the server and wraps children with its provider. For SSR entrypoints that use `createStore` instead of `hydrateStore`.

**Example:**
```reason
let result =
  StoreBuilder.Bootstrap.withCreatedProvider(
    ~createStore=TodoStore.createStore,
    ~provider=TodoStore.Context.Provider.make,
    ~initialState=preloadedState,
    ~children=<App />,
  );
```

### Event Listeners

#### `Events.listen`

```reason
let listen: (store_event('action) => unit) => listener_id;
```

Subscribe to typed store events. Returns a listener id for use with `unlisten`.

#### `Events.unlisten`

```reason
let unlisten: listener_id => unit;
```

Remove a previously registered listener.

**Lifecycle Pattern:**
```reason
React.useEffect0(() => {
  let listenerId =
    VideoChatStore.Events.listen(event => {
      switch (event) {
      | MediaEvent(json) => handleMedia(json)
      | CustomEvent(json) => handleCustom(json)
      | ActionAcked({actionId, action}) => handleAck(actionId, action)
      | ActionFailed({actionId, message}) => handleError(actionId, message)
      | _ => ()
      }
    });
  Some(() => VideoChatStore.Events.unlisten(listenerId));
});
```

**Important:** Always call `Events.unlisten` in your effect cleanup to prevent memory leaks and stale callbacks.

#### Synced Runtime Ordering Contract

- Confirmed snapshot application completes before any snapshot-adjacent notifications fire
- Confirmed patch application completes before any patch-adjacent notifications fire
- `ActionAcked` and `ActionFailed` are emitted after ledger status updates complete
- `Open`, `Close`, `Reconnect`, and `ConnectionError` are emitted from the runtime-owned sync layer
- Raw `snapshot` and `patch` frames are intentionally not the primary public event model

### IndexedDB Behavior

Both runtime builders use IndexedDB:

- `confirmed_state` stores `{scopeKey, value, timestamp}`
- Synced runtimes also use `actions` for queued typed actions and ack state

`storeName` identifies the IndexedDB database and `scopeKeyOfState` identifies the logical store instance within that database.

### StoreCrud

Generic CRUD helpers for realtime patch handling. Used internally by `DefineCrud` but available for custom implementations.

#### `StoreCrud.patch`

```reason
type patch('row) =
  | Upsert('row)
  | Delete(string);
```

#### `StoreCrud.decodePatch`

```reason
let decodePatch:
  (~table: string, ~decodeRow: StoreJson.json => 'row, unit) =>
  StorePatch.decoder(patch('row));
```

#### `StoreCrud.upsert`

```reason
let upsert: (~getId: 'row => string, array('row), 'row) => array('row);
```

#### `StoreCrud.remove`

```reason
let remove: (~getId: 'row => string, array('row), string) => array('row);
```

#### `StoreCrud.updateOfPatch`

```reason
let updateOfPatch:
  (~getId: 'row => string,
   ~getItems: 'config => array('row),
   ~setItems: ('config, array('row)) => 'config,
   patch('row)) =>
  'config => 'config;
```

### Projection Functions

#### `StoreBuilder.projected`

```reason
let projected: (
  ~derive: Derive.t=?,
  ~project: 'a => 'b,
  ~serverSource: 'a,
  ~fromStore: store => 'a,
  ~select: 'b => 'c,
  unit,
) => 'c;
```

Creates a derived value that updates when source changes.

**Parameters:**
- `~project`: Function to transform source
- `~serverSource`: Initial value for SSR
- `~fromStore`: Extractor from store
- `~select`: Selector for final value

#### `StoreBuilder.current`

```reason
let current: (
  ~derive: Derive.t=?,
  ~client: unit => 'a,
  ~server: unit => 'a,
  unit,
) => 'a;
```

Returns different values on client vs server.

#### `StoreBuilder.derived`

```reason
let derived: (
  ~derive: option(Tilia.Core.deriver('store))=?,
  ~client: 'store => 'a,
  ~server: unit => 'a,
  unit,
) => 'a;
```

Creates a derived value with SSR/hydration support.

### StorePatch Functions

#### `StorePatch.compose`

```reason
let compose: list(decoder('a)) => StoreJson.json => option('a);
```

Composes multiple patch decoders.

#### `StorePatch.Pg.decodeAs`

```reason
let decodeAs: (
  ~table: string,
  ~decodeRow: StoreJson.json => 'row,
  ~insert: 'row => 'patch,
  ~update: 'row => 'patch,
  ~delete: string => 'patch,
  unit,
) => decoder('patch);
```

Creates PostgreSQL change decoder. For standard CRUD, prefer `StoreCrud.decodePatch` which wraps this with `Upsert`/`Delete` variants.

## Components

### UniversalComponents.Document

```reason
[@react.component]
let make: (
  ~title: string,
  ~meta: array(metaTag)=?,
  ~links: array(linkTag)=?,
  ~scripts: array(scriptTag)=?,
  ~children: React.element,
) => React.element;
```

Document wrapper component.

### UniversalComponents.NoSSR

```reason
[@react.component]
let make: (~children: React.element) => React.element;
```

Client-only rendering wrapper.

### UniversalComponents.Suspense

```reason
[@react.component]
let make: (
  ~fallback: React.element,
  ~children: React.element,
) => React.element;
```

Suspense boundary component.

### UniversalComponents.ErrorBoundary

```reason
[@react.component]
let make: (
  ~fallback: error => React.element,
  ~children: React.element,
) => React.element;
```

Error boundary component.

## Lucide Icons

All Lucide icons are available as components with these props:

```reason
[@react.component]
let make: (
  ~size: int=?,
  ~color: string=?,
  ~strokeWidth: float=?,
  ~className: string=?,
  ~ariaLabel: string=?,
) => React.element;
```

**Examples:**
- `<Home size=24 />`
- `<User color="#333" />`
- `<Settings strokeWidth=1.5 className="icon" />`

## Real-time Middleware

### Types

#### `Middleware.t`

```reason
type t;
```

Middleware handle.

#### `Adapter.packed`

```reason
type packed = Pack : (module S with type t = 'a) * 'a -> packed
```

Packed adapter value.

### Functions

#### `Middleware.create`

```reason
let create: (
  ~adapter: Adapter.packed,
  ~resolve_subscription: Dream.request => string => string option Lwt.t,
  ~load_snapshot: Dream.request => string => string Lwt.t,
  ?handle_mutation: Dream.request => action_id:string => Yojson.Basic.t => (unit, string) result Lwt.t,
  ?dispatch_mutation: (module Caqti_lwt.CONNECTION) => mutation_name:string => Yojson.Basic.t => (unit, string) result Lwt.t option,
  unit,
) => Middleware.t;
```

Creates middleware instance. `~handle_mutation` is optional and receives JSON mutation frames in the form `{type: "mutation", actionId, action}`. If `~dispatch_mutation` is provided, the middleware tries it first for every mutation; only when it returns `None` does the frame fall through to `~handle_mutation`.

#### `Middleware.route`

```reason
let route: (string, Middleware.t) => Dream.route;
```

Builds the websocket route for `Dream.router`.

#### `Middleware.broadcast`

```reason
let broadcast: (Middleware.t, string, string) => Lwt.t(unit);
```

Broadcasts a payload string to all connected clients.

**Test coverage:** Native protocol tests for ping/select/ack/error/media/detach behavior live under `packages/reason-realtime/dream-middleware/test`. See `docs/testing.md` for commands and the full case list.

### Realtime Client Socket

#### `RealtimeClient.Socket.sendAction`

```reason
let sendAction: (~actionId: string, ~action: StoreJson.json) => unit;
```

Send a typed JSON action over the currently active realtime websocket.

#### `Adapter.pack`

```reason
let pack: (module Adapter.S with type t = 'a) -> 'a -> Adapter.packed;
```

Pack an adapter implementation.

#### `Adapter.start`

```reason
let start: Adapter.packed -> unit Lwt.t;
```

Start a packed adapter.

#### `Adapter.stop`

```reason
let stop: Adapter.packed -> unit Lwt.t;
```

Stop a packed adapter.

#### `Adapter.subscribe`

```reason
let subscribe: Adapter.packed -> channel:string -> handler:(string -> unit Lwt.t) -> unit Lwt.t;
```

Subscribe a packed adapter to a channel.

#### `Adapter.unsubscribe`

```reason
let unsubscribe: Adapter.packed -> channel:string -> unit Lwt.t;
```

Unsubscribe a packed adapter from a channel.

## PostgreSQL Notify Adapter

### Types

#### `Pgnotify_adapter.t`

```reason
type t;
```

PostgreSQL adapter handle.

### Functions

#### `Pgnotify_adapter.create`

```reason
let create: (~db_uri: string, unit) => Pgnotify_adapter.t;
```

Creates PostgreSQL notify adapter.

**Parameters:**
- `~db_uri`: PostgreSQL connection string

#### `Pgnotify_adapter.start`

```reason
let start: Pgnotify_adapter.t => Lwt.t(unit);
```

Starts adapter polling.

#### `Pgnotify_adapter.stop`

```reason
let stop: Pgnotify_adapter.t => Lwt.t(unit);
```

Stops adapter and closes connections.

#### `Pgnotify_adapter.subscribe`

```reason
let subscribe:
  (Pgnotify_adapter.t, ~channel: string, ~handler: (string => unit Lwt.t)) => Lwt.t(unit);
```

Register a handler for channel updates.

#### `Pgnotify_adapter.unsubscribe`

```reason
let unsubscribe: (Pgnotify_adapter.t, ~channel: string) => Lwt.t(unit);
```

Stop listening on a channel and remove handlers.

**Test coverage:** DB-backed integration tests for subscribe/delivery/unsubscribe live under `packages/reason-realtime/pgnotify-adapter/test`. See `docs/testing.md` for setup and commands.

## realtime-schema PPX Generated Helpers

The `[%realtime_schema "..."]` PPX generates per-query and per-mutation modules with ready-to-use Caqti helpers.

### Table modules

Every `@table` emits a module under `RealtimeSchema.Tables.*` with a `caqti_type` for the table record:

```reason
module Tables.Inventory = struct
  let name = "inventory"
  let id_column = Some "id"
  let composite_key = []
  let caqti_type = Caqti_type.product(...) [@@platform native]
end
```

### Query modules

Every `@query` emits a module under `RealtimeSchema.Queries.*` with:

```reason
module Queries.GetInventoryList = struct
  let name = "get_inventory_list"
  let sql = "SELECT ..."
  let json_columns = ["period_list"]
  let handler = Sql

  type row = {
    id : string;
    premise_id : string;
    name : string;
    description : string;
    quantity : int;
    period_list : string;
  } [@@platform native]

  let caqti_type = Caqti_type.product(...) [@@platform native]
  let param_type = Caqti_type.string [@@platform native]
  let request row_type = Caqti_request.Infix.(param_type ->* row_type)(sql) [@@platform native]
  let find_request row_type = Caqti_request.Infix.(param_type ->? row_type)(sql) [@@platform native]
  let collect (module Db : Caqti_lwt.CONNECTION) row_type params = ... [@@platform native]
  let find_opt (module Db : Caqti_lwt.CONNECTION) row_type params = ... [@@platform native]
end
```

### Mutation modules

Every `@mutation` emits a module under `RealtimeSchema.Mutations.*` with:

```reason
module Mutations.AddTodo = struct
  let name = "add_todo"
  let sql = "INSERT INTO todos ..."
  let handler = Sql

  let param_type = Caqti_type.t2(Caqti_type.string, Caqti_type.string) [@@platform native]
  let request = Caqti_request.Infix.(param_type ->. Caqti_type.unit)(sql) [@@platform native]
  let exec (module Db : Caqti_lwt.CONNECTION) params = ... [@@platform native]

  (* For @handler sql mutations only *)
  let dispatch (module Db : Caqti_lwt.CONNECTION) action = ... [@@platform native]
end
```

For `@handler sql` mutations, the PPX also generates a router that the middleware can use for auto-dispatch:

```reason
let dispatch_mutation (module Db : Caqti_lwt.CONNECTION) ~mutation_name action = ...
```

Wire it into the middleware so standard SQL mutations do not require a custom `handle_mutation`:

```reason
Middleware.create(
  ~adapter,
  ~resolve_subscription,
  ~load_snapshot,
  ~dispatch_mutation:RealtimeSchema.dispatch_mutation,
  (),
)
```

Only mutations that return `None` from `dispatch_mutation` fall through to `~handle_mutation`.

**Important:** All Caqti bindings are decorated with `[@@platform native]` so the shared schema file compiles under both Melange (JS) and native OCaml targets.

## universal-reason-react/intl

Universal internationalization library with `Intl.NumberFormatter` and `Intl.DateTimeFormatter` for both server (via ICU4C) and client (via native `Intl`).

**Packages:**
- JS target: `resync.universal_reason_react_intl_js` (Melange)
- Native target: `resync.universal_reason_react_intl_native` (OCaml native)

This replaces the old separate `icu-numberformatter` and `icu-datetimeformatter` packages with a unified API that works identically on both server and client.

### Intl.NumberFormatter

Number formatting for currency, decimal, and percent values.

#### Types

```reason
module Intl.NumberFormatter.Style: {
  type t = Decimal | Currency | Percent;
};

type Intl.NumberFormatter.part = {
  type_: string,
  value: string,
};

type Intl.NumberFormatter.options = {
  locale: option(string),
  style: option(Intl.NumberFormatter.Style.t),
  currency: option(string),
  minimumFractionDigits: option(int),
  maximumFractionDigits: option(int),
  useGrouping: option(bool),
};
```

#### Functions

```reason
let Intl.NumberFormatter.make: Intl.NumberFormatter.options => Intl.NumberFormatter.t;

let Intl.NumberFormatter.format: (Intl.NumberFormatter.t, float) => string;

let Intl.NumberFormatter.formatToParts: (Intl.NumberFormatter.t, float) => list(Intl.NumberFormatter.part);

let Intl.NumberFormatter.formatWithOptions: (Intl.NumberFormatter.options, float) => string;

let Intl.NumberFormatter.formatToPartsWithOptions: (Intl.NumberFormatter.options, float) => list(Intl.NumberFormatter.part);
```

#### Example

```reason
let formatter = Intl.NumberFormatter.make({
  locale: Some("en-US"),
  style: Some(Intl.NumberFormatter.Style.Currency),
  currency: Some("USD"),
  minimumFractionDigits: None,
  maximumFractionDigits: None,
  useGrouping: None,
});

let price = formatter->Intl.NumberFormatter.format(1234.56);
// "price" is "$1,234.56"
```

### Intl.DateTimeFormatter

Date/time formatting for various locales and styles.

#### Types

```reason
module Intl.DateTimeFormatter.Style: {
  type t = Full | Long | Medium | Short;
};

module Intl.DateTimeFormatter.Text: {
  type t = Narrow | Short | Long;
};

module Intl.DateTimeFormatter.Numeric: {
  type t = Numeric | TwoDigit;
};

module Intl.DateTimeFormatter.Month: {
  type t = Numeric | TwoDigit | Narrow | Short | Long;
};

module Intl.DateTimeFormatter.HourCycle: {
  type t = H11 | H12 | H23 | H24;
};

module Intl.DateTimeFormatter.TimeZoneName: {
  type t =
    | Short
    | Long
    | ShortOffset
    | LongOffset
    | ShortGeneric
    | LongGeneric;
};

type Intl.DateTimeFormatter.part = {
  type_: string,
  value: string,
};

type Intl.DateTimeFormatter.options = {
  locale: option(string),
  timeZone: option(string),
  dateStyle: option(Intl.DateTimeFormatter.Style.t),
  timeStyle: option(Intl.DateTimeFormatter.Style.t),
  weekday: option(Intl.DateTimeFormatter.Text.t),
  era: option(Intl.DateTimeFormatter.Text.t),
  year: option(Intl.DateTimeFormatter.Numeric.t),
  month: option(Intl.DateTimeFormatter.Month.t),
  day: option(Intl.DateTimeFormatter.Numeric.t),
  hour: option(Intl.DateTimeFormatter.Numeric.t),
  minute: option(Intl.DateTimeFormatter.Numeric.t),
  second: option(Intl.DateTimeFormatter.Numeric.t),
  fractionalSecondDigits: option(int),
  timeZoneName: option(Intl.DateTimeFormatter.TimeZoneName.t),
  hour12: option(bool),
  hourCycle: option(Intl.DateTimeFormatter.HourCycle.t),
};
```

#### Functions

```reason
let Intl.DateTimeFormatter.make: Intl.DateTimeFormatter.options => Intl.DateTimeFormatter.t;

let Intl.DateTimeFormatter.format: (Intl.DateTimeFormatter.t, float) => string;

let Intl.DateTimeFormatter.formatToParts: (Intl.DateTimeFormatter.t, float) => list(Intl.DateTimeFormatter.part);

let Intl.DateTimeFormatter.formatWithOptions: (Intl.DateTimeFormatter.options, float) => string;

let Intl.DateTimeFormatter.formatToPartsWithOptions: (Intl.DateTimeFormatter.options, float) => list(Intl.DateTimeFormatter.part);
```

#### Example

```reason
let formatter = Intl.DateTimeFormatter.make({
  locale: Some("en-US"),
  timeZone: Some("UTC"),
  dateStyle: Some(Intl.DateTimeFormatter.Style.Short),
  timeStyle: None,
  weekday: None,
  era: None,
  year: None,
  month: None,
  day: None,
  hour: None,
  minute: None,
  second: None,
  fractionalSecondDigits: None,
  timeZoneName: None,
  hour12: None,
  hourCycle: None,
});

let date = formatter->Intl.DateTimeFormatter.format(1608434596738.0);
// "date" is "12/20/2020"
```

### Notes

- **Universal API**: Same API works on both JS (via browser `Intl`) and native (via ICU4C)
- **CamelCase naming**: Uses camelCase for consistency with JavaScript `Intl` API
- **No platform switching**: No need for `switch%platform` - the universal package handles target differences internally
- **formatToParts**: Both formatters support `formatToParts` for styled number/date formatting
- **Caching**: Formatters are cached in-process for reuse on the native side

## ocaml-icu4c

Low-level OCaml bindings for ICU4C (International Components for Unicode). Used internally by the intl package for native targets.

**Package:** `resync.ocaml_icu4c`

### Modules

- `Icu4c_bindings` - Raw C bindings with version suffix handling
- `Icu4c_strings` - UTF-8/UTF-16 conversion utilities
- `Icu4c_locale` - Locale and timezone normalization
- `Icu4c_number` - Number formatting
- `Icu4c_datetime` - DateTime formatting

### Notes

- This is a low-level package primarily used by `universal-reason-react/intl`
- ICU discovery and symbol versioning handled at configure-time
- Wrapped module structure to avoid naming collisions

## Type Definitions

### Common Types

```reason
// JSON codecs
type 'a jsonCodec = {
  to_json: 'a => Js.Json.t,
  of_json: Js.Json.t => 'a,
};

// Async result
type 'a asyncResult = Lwt.t(Belt.Result.t('a, string));

// Page component
type pageComponent = {
  make: (~params: Js.Dict.t(string)) => React.element,
};

// Layout component  
type layoutComponent = {
  make: (~children: React.element) => React.element,
};
```

### Router Types

```reason
type matchResult('state) = {
  pathname: string,
  params: Params.t,
  searchParams: SearchParams.t,
  routes: list(routeConfig('state)),
};

// Metadata resolvers
type titleResolver = (
  ~pathname: string,
  ~params: Params.t,
  ~searchParams: SearchParams.t,
) => string;

type titleResolverWithState('state) = (
  ~pathname: string,
  ~params: Params.t,
  ~searchParams: SearchParams.t,
  ~state: 'state,
) => string;

type headTagsResolver = (
  ~pathname: string,
  ~params: Params.t,
  ~searchParams: SearchParams.t,
) => list(headTag);

type headTagsResolverWithState('state) = (
  ~pathname: string,
  ~params: Params.t,
  ~searchParams: SearchParams.t,
  ~state: 'state,
) => list(headTag);
```

### Store Types

```reason
type source('a) = {
  get: unit => 'a,
  set: 'a => unit,
  update: ('a => 'a) => unit,
  subscribe: ('a => unit) => unsubscribe,
}
and unsubscribe = unit => unit;

type derive('a, 'b) = {
  project: 'a => 'b,
  compare: ('b, 'b) => bool,
};
```

## Error Handling

### Common Error Types

```reason
type routerError =
  | RouteNotFound(string)
  | InvalidParams(string)
  | NavigationError(string);

type storeError =
  | HydrationError(string)
  | SerializationError(string)
  | PatchDecodeError(string);

type realtimeError =
  | ConnectionError(string)
  | AuthenticationError(string)
  | SubscriptionError(string);
```

### Error Handling Patterns

```reason
// Router error handling
switch (route) {
| Some(route) => render(route)
| None => <NotFoundPage />
};

// Store error handling
try (Store.hydrateStore()) {
| Store.HydrationError(msg) => {
    Js.Console.error("Hydration failed: " ++ msg);
    Store.createStore(defaultState);
  }
};

// Async error handling
let%lwt result = fetchData();
switch (result) {
| Ok(data) => Lwt.return(data)
| Error(err) => Lwt.fail(Failure(err))
};
```

## Constants

### Default Values

```reason
// Router
let defaultDocumentTitle = "Universal Reason App";
let defaultNotFoundStatus = 404;

// Store
let defaultStateElementId = "__store_state__";
let defaultPersistenceKey = "universal_store_v1";
```
