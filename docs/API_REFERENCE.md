# API Reference

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.


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

#### `useNavigate`

```reason
let useNavigate: unit => (~to_: string, ~search: string=?, unit) => unit;
```

Hook for programmatic navigation.

**Example:**
```reason
let navigate = UniversalRouter.useNavigate();
navigate(~to_="/dashboard", ());
```

#### `useParams`

```reason
let useParams: unit => Js.Dict.t(string);
```

Hook to access route parameters.

**Example:**
```reason
let params = UniversalRouter.useParams();
let id = params |. Js.Dict.get("id") |. Belt.Option.getExn;
```

#### `useLocation`

```reason
let useLocation: unit => location;
```

Hook to access current location.

**Type:**
```reason
type location = {
  pathname: string,
  search: string,
  hash: string,
};
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

#### `UniversalRouterDream.contextRouteRoot`

```reason
let contextRouteRoot: serverContext('state) => string;
```

Extracts route root from server context.

#### `UniversalRouterDream.contextPath`

```reason
let contextPath: serverContext => string;
```

Extracts request path from server context.

#### `UniversalRouterDream.contextSearch`

```reason
let contextSearch: serverContext => string;
```

Extracts query string from server context.

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

### StoreBuilder.Runtime.Make

Module functor for creating runtime stores.

**Signature:**
```reason
module Runtime = StoreBuilder.Runtime.Make({
  type config;
  type patch;
  type payload;
  type store;
  type subscription;

  let emptyStore: unit => store;
  let stateElementId: string;
  let payloadOfConfig: config => payload;
  let configOfPayload: payload => config;
  let subscriptionOfConfig: config => option(subscription);
  let encodeSubscription: subscription => Js.Json.t;
  let updatedAtOf: config => option(Js.Date.t);
  let eventUrl: string;
  let baseUrl: string;
  let updateOfPatch: patch => config => config;
  let decodePatch: Js.Json.t => option(patch);
  let config_of_json: Js.Json.t => config;
  let config_to_json: config => Js.Json.t;
  let payload_of_json: Js.Json.t => payload;
  let payload_to_json: payload => Js.Json.t;
});
```

**Exports:**
- `type t`: The store type
- `type config`: Configuration type
- `type payload`: Payload type
- `let createStore: config => t`: Create new store
- `let hydrateStore: unit => t`: Hydrate from SSR
- `let serializeState: config => string`: Serialize for SSR
- `let useStore: unit => t`: Hook to access store
- `let useUpdate: unit => (t => t) => unit`: Hook to update store
- `let Context: React.context(t)`: React context

### StoreBuilder.Persisted.Make

Module functor for creating persisted stores.

**Signature:**
```reason
module Persisted = StoreBuilder.Persisted.Make({
  type t;
  let key: string;
  let default: unit => t;
  let encode: t => Js.Json.t;
  let decode: Js.Json.t => t;
});
```

**Exports:**
- `type t`: The store type
- `let useStore: unit => t`: Hook to access store
- `let useUpdate: unit => (t => t) => unit`: Hook to update store
- `let Context: React.context(t)`: React context
- `let clear: unit => unit`: Clear persisted data

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

### StorePatch Functions

#### `StorePatch.compose`

```reason
let compose: list(decoder('a)) => Js.Json.t => option('a);
```

Composes multiple patch decoders.

#### `StorePatch.Pg.decodeAs`

```reason
let decodeAs: (
  ~table: string,
  ~decodeRow: Js.Json.t => 'row,
  ~insert: 'row => 'patch,
  ~update: 'row => 'patch,
  ~delete: string => 'patch,
  unit,
) => decoder('patch);
```

Creates PostgreSQL change decoder.

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
) => Middleware.t;
```

Creates middleware instance.

#### `Middleware.route`

```reason
let route: (string, Middleware.t) => Dream.handler;
```

Builds a Dream handler for a websocket route.

#### `Middleware.broadcast`

```reason
let broadcast: (Middleware.t, string, string) => Lwt.t(unit);
```

Broadcasts a payload string to all connected clients.

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
type location = {
  pathname: string,
  search: string,
  hash: string,
  state: option(Js.Json.t),
};

type match = {
  params: Js.Dict.t(string),
  isExact: bool,
  path: string,
  url: string,
};

type historyAction =
  | Push
  | Replace
  | Pop;

// Metadata resolvers
type titleResolver = (
  ~path: string,
  ~params: Params.t,
  ~query: Query.t,
) => string;

type titleResolverWithState('state) = (
  ~path: string,
  ~params: Params.t,
  ~query: Query.t,
  ~state: 'state,
) => string;

type headTagsResolver = (
  ~path: string,
  ~params: Params.t,
  ~query: Query.t,
) => list(headTag);

type headTagsResolverWithState('state) = (
  ~path: string,
  ~params: Params.t,
  ~query: Query.t,
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
