# universal-reason-react/store

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.


Opinionated Tilia-backed store tooling for universal (SSR + client) Reason React applications.

## Overview

The Universal Store provides a complete state management solution that works seamlessly across server and client:

- **Server-Side Rendering**: Bootstrap initial state on the server
- **Client Hydration**: Hydrate server state without data fetching
- **Persistence**: Automatically persist state to localStorage
- **Real-time Sync**: Live updates and mutation commands via WebSocket
- **Reactive Updates**: Tilia-based reactive state management

## Core Concepts

### Source State Pattern

Rather than managing state with useState or useReducer, the store uses a **source state** pattern:

- **Source**: Single source of truth (plain data structure)
- **Projections**: Derived values computed from source
- **Updates**: Functions that transform source state

This makes state changes predictable and enables features like time-travel debugging.

### Store Types

1. **Runtime Store**: Server-rendered with real-time sync
2. **Persisted Store**: Client-only with localStorage persistence

## Quick Start

### Basic Runtime Store

```reason
// Store.re
[@deriving json]
type config = Model.t;

type subscription = RealtimeSubscription.t;

type patch = StoreCrud.patch(Model.InventoryItem.t);

[@deriving json]
type payload = {
  config: config,
  unit: PeriodList.Unit.t,
};

type store = {
  premise_id: string,
  config: config,
  period_list: array(Model.Pricing.period),
  unit: PeriodList.Unit.t,
};

module Runtime = StoreBuilder.Runtime.Make({
  type nonrec config = config;
  type nonrec patch = patch;
  type nonrec payload = payload;
  type nonrec store = store;
  type nonrec subscription = subscription;

  let emptyStore: store = {
    premise_id: "",
    config: Model.SSR.empty,
    period_list: [||],
    unit: PeriodList.Unit.defaultState,
  };
  let stateElementId = "initial-store";

  let payloadOfConfig = (config: config): payload => {
    config,
    unit:
      switch%platform (Runtime.platform) {
      | Server => PeriodList.Unit.defaultState
      | Client => PeriodList.Unit.get()
      },
  };
  let configOfPayload = (payload: payload): config => payload.config;

  let config_of_json = config_of_json;
  let config_to_json = config_to_json;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;

  let decodePatch =
    StorePatch.compose([
      StoreCrud.decodePatch(
        ~table=RealtimeSchema.table_name("inventory"),
        ~decodeRow=Model.InventoryItem.of_json,
        (),
      ),
    ]);

  let updateOfPatch = StoreCrud.updateOfPatch(
    ~getId=(item: Model.InventoryItem.t) => item.id,
    ~getItems=(config: config) => config.inventory,
    ~setItems=(config: config, items) => {...config, inventory: items},
  );

  let subscriptionOfConfig = (config: config): option(subscription) =>
    switch (config.premise) {
    | Some(premise) => Some(RealtimeSubscription.premise(premise.id))
    | None => None
    };
  let encodeSubscription = RealtimeSubscription.encode;

  let updatedAtOf = (config: config) =>
    switch (config.premise) {
    | Some(premise) => premise.updated_at->Js.Date.getTime
    | None => 0.0
    };
  let eventUrl = Constants.event_url;
  let baseUrl = Constants.base_url;
});

include (
  Runtime:
    StoreBuilder.Runtime.Exports
      with type config := config
      and type payload := payload
      and type t := store
);

type t = store;

module Context = Runtime.Context;
```

When the app runs on a port other than the default realtime demo port, prefer an app-level `Constants.base_url` over `RealtimeClient.Socket.defaultBaseUrl`.

### Sending Named Mutations

Runtime stores can also send named websocket mutations and wait for the server-triggered patch to update the source state:

```reason
RealtimeClient.Socket.sendMutation(
  "add_todo",
  StoreJson.stringify(add_todo_input_to_json, {list_id, text}),
);
```

This is the non-optimistic path: the UI sends a command, the server executes the write, and the store updates when the patch comes back over the websocket.

### Using the Store

```reason
// App.re
[@react.component]
let make = () => {
  let store = Store.useStore();
  
  <div>
    {store.config.inventory
     |> Array.to_list
     |> List.map(item =>
       <ItemCard key=item.id item />
     )
     |> React.array}
  </div>;
};
```

### Persisted Client Store

```reason
// CartStore.re
[@deriving json]
type cartItem = {
  productId: string,
  quantity: int,
};

[@deriving json]
type config = {
  items: array(cartItem),
};

type payload = config;

type store = config;

module Persisted = StoreBuilder.Persisted.Make({
  type config = config;
  type payload = payload;
  type store = store;

  let storageKey = "shopping_cart";
  let emptyStore: store = {items: [||]};
  let payloadOfConfig = (config: config): payload => config;
  let configOfPayload = (payload: payload): config => payload;
  let payloadOfStore = (store: store): payload => store;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
  let transformConfig = (config: config): config => config;
  let makeStore =
      (~config: config, ~payload: payload, ~derive: Tilia.Core.deriver(store)=?, ()) =>
    StoreComputed.make(
      ~client=derive => config,
      ~server=() => config,
    );
});

include (
  Persisted:
    StoreBuilder.Persisted.Exports
      with type config := config
      and type payload := payload
      and type t := store
);

type t = store;
module Context = Persisted.Context;
```

## API Reference

### StoreBuilder.Runtime.Make

Creates a runtime store with SSR and real-time sync capabilities.

Define all values inline inside the functor body — no need to define them at the top level and pass them through:

```reason
module Runtime = StoreBuilder.Runtime.Make({
  type nonrec config = config;
  type nonrec patch = patch;
  type nonrec payload = payload;
  type nonrec store = store;
  type nonrec subscription = subscription;

  let emptyStore: store = ...;
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

### StoreBuilder.Persisted.Make

Creates a persisted client-side store.

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

### Hooks

#### `useStore`

```reason
let useStore: unit => store;
```

Access the current store value.

#### `useUpdate`

```reason
let useUpdate: unit => (store => store) => unit;
```

Get a function to update the store.

#### `useSelector`

```reason
let useSelector: 'a. (store => 'a) => 'a;
```

Select a specific value from the store (optimized for minimal re-renders).

#### `useHydrateStore`

```reason
let useHydrateStore: unit => store;
```

Hydrate the store from server-rendered state (client-side only).

### StoreCrud

Generic CRUD helpers for realtime patch handling. Eliminates per-table patch type definitions, upsert/delete logic, and patch decoder wiring.

#### `StoreCrud.patch`

```reason
type patch('row) =
  | Upsert('row)
  | Delete(string);
```

Polymorphic patch type. Use as `type patch = StoreCrud.patch(MyRow.t)`.

#### `StoreCrud.decodePatch`

```reason
let decodePatch:
  (~table: string, ~decodeRow: StoreJson.json => 'row, unit) =>
  StorePatch.decoder(patch('row));
```

Creates a patch decoder for a single table. Wraps `StorePatch.Pg.decodeAs` with `Upsert`/`Delete` mapping.

```reason
StoreCrud.decodePatch(
  ~table=RealtimeSchema.table_name("inventory"),
  ~decodeRow=Model.InventoryItem.of_json,
  (),
)
```

#### `StoreCrud.upsert`

```reason
let upsert: (~getId: 'row => string, array('row), 'row) => array('row);
```

Generic array upsert by ID. Replaces existing item if found, appends otherwise.

#### `StoreCrud.remove`

```reason
let remove: (~getId: 'row => string, array('row), string) => array('row);
```

Generic array filter by ID.

#### `StoreCrud.updateOfPatch`

```reason
let updateOfPatch:
  (~getId: 'row => string,
   ~getItems: 'config => array('row),
   ~setItems: ('config, array('row)) => 'config,
   patch('row)) =>
  'config => 'config;
```

Returns a `config => config` updater for a single-table patch. Wires `Upsert` to `StoreCrud.upsert` and `Delete` to `StoreCrud.remove`.

```reason
let updateOfPatch = StoreCrud.updateOfPatch(
  ~getId=(item: Model.InventoryItem.t) => item.id,
  ~getItems=(config: config) => config.inventory,
  ~setItems=(config: config, items) => {...config, inventory: items},
);
```

### Projections

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

Create a derived value that updates when the source changes.

**Example:**

```reason
let totalPrice =
  StoreBuilder.projected(
    ~project=items =>
      items |> List.fold_left((acc, item) =>
        acc +. item.price *. float_of_int(item.quantity),
        0.0
      ),
    ~serverSource=initialItems,
    ~fromStore=store => store.items,
    ~select=price => price,
    (),
  );
```

#### `StoreBuilder.current`

```reason
let current: (
  ~derive: Derive.t=?,
  ~client: unit => 'a,
  ~server: unit => 'a,
  unit,
) => 'a;
```

Get different values on client vs server.

**Example:**

```reason
let theme =
  StoreBuilder.current(
    ~client=() => getLocalStorageTheme(),
    ~server=() => "light",
    (),
  );
```

## Advanced Patterns

### Computed Values with Caching

```reason
// Inside Runtime.Make, inside makeStore:
let filteredItems =
  StoreBuilder.projected(
    ~derive?,
    ~project=(config: config) =>
      config.items |> Array.to_list |> List.filter(item =>
        String.contains(item.name, filter)
      ) |> Array.of_list,
    ~serverSource=config,
    ~fromStore=store => store.config,
    ~select=items => items,
    (),
  );
```

### Optimistic Updates

```reason
let addItemOptimistic = (item: Item.t) => {
  let update = Store.useUpdate();
  let revertTimeout = React.useRef(None);
  
  update(store => {
    ...store,
    config: StoreCrud.upsert(
      ~getId=(i: Item.t) => i.id,
      store.config.items,
      item,
    ),
  });
  
  Api.addItem(item)
  |> Js.Promise.then_(() => {
    revertTimeout.current->Belt.Option.forEach(Js.Global.clearTimeout);
    Js.Promise.resolve();
  })
  |> Js.Promise.catch(_ => {
    revertTimeout.current = Some(
      Js.Global.setTimeout(() => {
        update(store => {
          ...store,
          config: StoreCrud.remove(
            ~getId=(i: Item.t) => i.id,
            store.config.items,
            item.id,
          ),
        });
      }, 3000)
    );
    Js.Promise.resolve();
  });
};
```

### Store Composition

Multiple stores are composed via nested React contexts:

```reason
// Index.re
let store = Store.hydrateStore();
let cartStore = CartStore.hydrateStore();

ReactDOM.hydrateRoot(
  root,
  <Store.Context.Provider value=store>
    <CartStore.Context.Provider value=cartStore>
      <App />
    </CartStore.Context.Provider>
  </Store.Context.Provider>,
);
```

### Real-time Sync Configuration

```reason
// Using StoreCrud for standard CRUD tables
let decodePatch =
  StorePatch.compose([
    StoreCrud.decodePatch(
      ~table=RealtimeSchema.table_name("items"),
      ~decodeRow=Item.of_json,
      (),
    ),
    // Multiple tables:
    StoreCrud.decodePatch(
      ~table=RealtimeSchema.table_name("users"),
      ~decodeRow=User.of_json,
      (),
    ),
  ]);

let updateOfPatch = StoreCrud.updateOfPatch(
  ~getId=(item: Item.t) => item.id,
  ~getItems=(config: config) => config.items,
  ~setItems=(config: config, items) => {...config, items},
);

// For multi-table patches with different config fields, use StorePatch.compose
// for decoding and a manual updateOfPatch:
let updateOfPatch = patch =>
  switch (patch) {
  | ItemsPatch(p) =>
    config => {
      ...config,
      items: switch (p) {
        | Upsert(newItem) => StoreCrud.upsert(~getId=(i => i.id), config.items, newItem)
        | Delete(id) => StoreCrud.remove(~getId=(i => i.id), config.items, id)
      },
    }
  | UsersPatch(p) =>
    config => {
      ...config,
      users: switch (p) {
        | Upsert(newUser) => StoreCrud.upsert(~getId=(u => u.id), config.users, newUser)
        | Delete(id) => StoreCrud.remove(~getId=(u => u.id), config.users, id)
      },
    }
  };
```

## Server-Side Integration

### Fetching Initial State

```reason
// EntryServer.re
let getServerState = (context: UniversalRouterDream.serverContext) => {
  let UniversalRouterDream.{basePath, request} = context;
  
  // Fetch data based on route
  let* items =
    Dream.sql(request, Database.Items.getAll());
  
  let* user =
    Dream.sql(request, Database.Users.getCurrent());
  
  let config: Store.config = {
    items,
    user,
  };
  
  Lwt.return(UniversalRouterDream.State(config));
};
```

### State Serialization

```reason
let render = (~context, ~serverState, ()) => {
  let store = Store.createStore(serverState);
  let serializedState = Store.serializeState(serverState);
  
  // Pass serializedState to document for hydration
  ...
};
```

## Client-Side Integration

### Hydration

```reason
// Index.re
let store = Store.hydrateStore();

ReactDOM.hydrateRoot(
  root,
  <Store.Context.Provider value=store>
    <App />
  </Store.Context.Provider>,
);
```

### Persistence

```reason
// CartStore.re
module Persisted = StoreBuilder.Persisted.Make({
  type config = config;
  type payload = payload;
  type store = store;
  
  let storageKey = "cart_v1";  // Version your keys!
  // ... see Persisted Client Store example above
});
```

## Best Practices

### 1. Keep Source State Serializable

✅ **Good:**
```reason
type config = {
  items: array(Item.t),
  userId: string,
};
```

❌ **Bad:**
```reason
type config = {
  items: array(Item.t),
  ref: React.ref(option(Dom.element)),
};
```

### 2. Normalize Complex Data

✅ **Good:**
```reason
type config = {
  users: array(User.t),
  posts: array(Post.t),
  currentUserId: string,
};
```

❌ **Bad:**
```reason
type config = {
  posts: array({
    ...postFields,
    author: User.t,  // Nested data duplicates user data
    comments: array(Comment.t),  // Deep nesting
  }),
};
```

### 2. Use Projections for Derived Data

✅ **Good:**
```reason
let itemCount =
  StoreBuilder.projected(
    ~project=(config: config) => Array.length(config.items),
    ~serverSource=config,
    ~fromStore=store => store.config,
    ~select=count => count,
    (),
  );
```

❌ **Bad:**
```reason
// Computing in every render
let itemCount = Array.length(store.config.items);
```

### 3. Version Your Persisted Stores

```reason
let key = "app_state_v2";  // Increment when schema changes
```

### 4. Handle Schema Migrations

```reason
let decode = (json: Js.Json.t) => {
  try {
    cart_of_json(json);
  } catch {
  | _ =>
    // Try old format
    tryMigrateV1(json);
  };
};
```

## Troubleshooting

### Hydration Mismatch

**Problem:** Client and server render different content

**Solutions:**
1. Ensure `client` and `server` branches in `StoreBuilder.current` return compatible values
2. Check that random values (timestamps, IDs) are consistent
3. Verify environment-specific code doesn't run during SSR

### Store Not Updating

**Problem:** UI doesn't reflect store changes

**Solutions:**
1. Ensure component uses `useStore` or `useSelector` hook
2. Check that update function returns a new object (not mutated)
3. Verify patch decoding is working correctly

### Persistence Not Working

**Problem:** Store doesn't persist to localStorage

**Solutions:**
1. Check browser console for quota exceeded errors
2. Ensure store data is JSON-serializable
3. Verify key doesn't collide with other stores

### Real-time Updates Not Received

**Problem:** Patches not updating the store

**Solutions:**
1. Check WebSocket connection is established
2. Verify subscription encoding matches server expectations
3. Ensure `decodePatch` correctly handles all patch types
4. Check browser console for patch parsing errors

## Examples

### Full E-commerce Store

See [demos/ecommerce/ui/src/Store.re](../demos/ecommerce/ui/src/Store.re) for a complete example including:
- Real-time inventory updates via `StoreCrud`
- Projected derived values (period list, premise ID)
- Persisted cart store with localStorage

### Todo App Store

```reason
// TodoStore.re
[@deriving json]
type todo = {
  id: string,
  text: string,
  completed: bool,
};

type patch = StoreCrud.patch(todo);

// Inside Runtime.Make functor:
let decodePatch =
  StorePatch.compose([
    StoreCrud.decodePatch(
      ~table=RealtimeSchema.table_name("todos"),
      ~decodeRow=todo_of_json,
      (),
    ),
  ]);

let updateOfPatch = StoreCrud.updateOfPatch(
  ~getId=(t: todo) => t.id,
  ~getItems=(config: config) => config.todos,
  ~setItems=(config: config, todos) => {...config, todos},
);
```

## Related Documentation

- [Dream Router Store Setup](dream-router-store-setup.md) - Complete integration example
- [Real-time Middleware](reason-realtime.dream-middleware.md) - WebSocket setup
- [PostgreSQL Adapter](reason-realtime.pgnotify-adapter.md) - Database notifications
- [Tilia Documentation](https://github.com/darklang/tilia) - Underlying state management
