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

### StoreBuilder.Runtime.Make

Module functor for creating runtime stores. Define all values inline inside the functor body.

**Signature:**
```reason
module Runtime = StoreBuilder.Runtime.Make({
  type config;
  type patch;
  type payload;
  type store;
  type subscription;

  let emptyStore: store;
  let stateElementId: string;
  let payloadOfConfig: config => payload;
  let configOfPayload: payload => config;
  let subscriptionOfConfig: config => option(subscription);
  let encodeSubscription: subscription => string;
  let updatedAtOf: config => float;
  let eventUrl: string;
  let baseUrl: string;
  let updateOfPatch: patch => config => config;
  let decodePatch: StoreJson.json => option(patch);
  let config_of_json: StoreJson.json => config;
  let config_to_json: config => StoreJson.json;
  let payload_of_json: StoreJson.json => payload;
  let payload_to_json: payload => StoreJson.json;
});
```

**Exports:**
- `type t`: The store type
- `type config`: Configuration type
- `type payload`: Payload type
- `let createStore: config => t`: Create new store
- `let hydrateStore: unit => t`: Hydrate from SSR
- `let serializeState: config => string`: Serialize for SSR
- `let serializeSnapshot: config => string`: Serialize for snapshot
- `let useStore: unit => t`: Hook to access store
- `let Context: React.context(t)`: React context

### StoreBuilder.Persisted.Make

Module functor for creating persisted stores.

**Signature:**
```reason
module Persisted = StoreBuilder.Persisted.Make({
  type config;
  type payload;
  type store;

  let storageKey: string;
  let emptyStore: store;
  let payloadOfConfig: config => payload;
  let configOfPayload: payload => config;
  let payloadOfStore: store => payload;
  let payload_of_json: StoreJson.json => payload;
  let payload_to_json: payload => StoreJson.json;
  let transformConfig: config => config;
  let makeStore:
    (~config: config, ~payload: payload, ~derive: Tilia.Core.deriver(store)=?, unit) =>
    store;
});
```

**Exports:**
- `type t`: The store type
- `let empty: t`: Empty store
- `let createStore: config => t`: Create new store
- `let hydrateStore: unit => t`: Hydrate from localStorage
- `let serializeState: config => string`: Serialize state
- `let persistPayload: payload => unit`: Persist payload
- `let persistStore: t => unit`: Persist store
- `let clear: unit => unit`: Clear persisted data
- `let Context: React.context(t)`: React context

### StoreCrud

Generic CRUD helpers for realtime patch handling.

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
  ?handle_mutation: Dream.request => string => string => unit Lwt.t,
  unit,
) => Middleware.t;
```

Creates middleware instance. `~handle_mutation` is optional and receives websocket commands in the form `mutation <name> <json>`.

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

### Realtime Client Socket

#### `RealtimeClient.Socket.sendMutation`

```reason
let sendMutation: (string, string) => unit;
```

Send a named mutation command over the currently active realtime websocket. The first argument is the mutation name and the second argument is the JSON payload string.

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
