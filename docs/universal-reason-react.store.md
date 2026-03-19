# universal-reason-react/store

Opinionated Tilia-backed store tooling for universal (SSR + client) Reason React applications.

## Overview

The Universal Store provides a complete state management solution that works seamlessly across server and client:

- **Server-Side Rendering**: Bootstrap initial state on the server
- **Client Hydration**: Hydrate server state without data fetching
- **Persistence**: Automatically persist state to localStorage
- **Real-time Sync**: Live updates via WebSocket
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
type config = {
  user: option(User.t),
  items: list(Item.t),
};

[@deriving json]
type payload = {
  user: option(User.t),
  items: list(Item.t),
};

type patch =
  | ItemAdd(Item.t)
  | ItemUpdate(string, Item.t)
  | ItemDelete(string);

type subscription = {
  userId: string,
};

let emptyStore = () => {
  user: None,
  items: [],
};

let stateElementId = "__store_state__";

let payloadOfConfig = (config: config): payload => {
  user: config.user,
  items: config.items,
};

let configOfPayload = (payload: payload): config => {
  user: payload.user,
  items: payload.items,
};

let subscriptionOfConfig = (config: config): option(subscription) =>
  config.user |> Option.map(user => {userId: user.id});

let encodeSubscription = (sub: subscription): Js.Json.t =>
  Json.Encode.object_([
    ("userId", Json.Encode.string(sub.userId)),
  ]);

let updatedAtOf = (config: config): option(Js.Date.t) =>
  // Return last update timestamp for sync
  None;

let eventUrl = "/_events";
let baseUrl = "ws://localhost:8080";

let updateOfPatch = (patch: patch) =>
  switch (patch) {
  | ItemAdd(item) =>
    config => {...config, items: [item, ...config.items]}
  | ItemUpdate(id, item) =>
    config => {
      ...config,
      items: config.items |> List.map(i =>
        i.id == id ? item : i
      ),
    }
  | ItemDelete(id) =>
    config => {
      ...config,
      items: config.items |> List.filter(i => i.id != id),
    }
  };

let decodePatch =
  StorePatch.compose([
    StorePatch.Pg.decodeAs(
      ~table="items",
      ~decodeRow=json => {
        let id = json |> Json.Decode.field("id", Json.Decode.string);
        let name = json |> Json.Decode.field("name", Json.Decode.string);
        {id, name};
      },
      ~insert=data => ItemAdd(data),
      ~update=data => ItemUpdate(data.id, data),
      ~delete=id => ItemDelete(id),
      (),
    ),
  ]);

module Runtime = StoreBuilder.Runtime.Make({
  type nonrec config = config;
  type nonrec patch = patch;
  type nonrec payload = payload;
  type nonrec store = config;
  type nonrec subscription = subscription;

  let emptyStore = emptyStore;
  let stateElementId = stateElementId;
  let payloadOfConfig = payloadOfConfig;
  let configOfPayload = configOfPayload;
  let subscriptionOfConfig = subscriptionOfConfig;
  let encodeSubscription = encodeSubscription;
  let updatedAtOf = updatedAtOf;
  let eventUrl = eventUrl;
  let baseUrl = baseUrl;
  let updateOfPatch = updateOfPatch;
  let decodePatch = decodePatch;
  let config_of_json = config_of_json;
  let config_to_json = config_to_json;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
});

include (
  Runtime:
    StoreBuilder.Runtime.Exports
      with type config := config
      and type payload := payload
      and type t := config
);

module Context = Runtime.Context;
```

### Using the Store

```reason
// App.re
[@react.component]
let make = () => {
  let store = Store.useStore();
  
  <div>
    {store.items
     |> List.map(item =>
       <ItemCard key={item.id} item />
     )
     |> React.array}
  </div>;
};

// ItemCard.re
[@react.component]
let make = (~item: Item.t) => {
  let updateItem = Store.useUpdate();
  
  let handleUpdate = () => {
    updateItem(item => {...item, name: "Updated Name"});
  };
  
  <div onClick={_ => handleUpdate()}>
    {React.string(item.name)}
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

type cart = {
  items: list(cartItem),
};

module Persisted = StoreBuilder.Persisted.Make({
  type t = cart;
  
  let key = "shopping_cart";
  let default = () => {items: []};
  let encode = cart_to_json;
  let decode = cart_of_json;
});

include Persisted;
module Context = Persisted.Context;

// Helper functions
let addItem = (cart, productId, quantity) => {
  let existing =
    cart.items |> List.find_opt(item => item.productId == productId);
  
  switch (existing) {
  | Some(item) =>
    let items =
      cart.items |> List.map(item =>
        item.productId == productId
          ? {...item, quantity: item.quantity + quantity}
          : item
      );
    {...cart, items};
  | None =>
    let newItem = {productId, quantity};
    {...cart, items: [newItem, ...cart.items]};
  };
};

let removeItem = (cart, productId) => {
  let items = cart.items |> List.filter(item => item.productId != productId);
  {...cart, items};
};
```

## API Reference

### StoreBuilder.Runtime.Make

Creates a runtime store with SSR and real-time sync capabilities.

```reason
module Runtime = StoreBuilder.Runtime.Make({
  type config;           // Server-side state type
  type patch;            // Update patch variant
  type payload;          // Serializable state type
  type store;            // Client store type
  type subscription;     // Real-time subscription config

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

### StoreBuilder.Persisted.Make

Creates a persisted client-side store.

```reason
module Persisted = StoreBuilder.Persisted.Make({
  type t;
  let key: string;
  let default: unit => t;
  let encode: t => Js.Json.t;
  let decode: Js.Json.t => t;
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
// Compute filtered items with memoization
let filteredItems = (~filter: string) => {
  StoreBuilder.projected(
    ~project=items =>
      items |> List.filter(item =>
        String.contains(item.name, filter)
      ),
    ~serverSource=store.items,
    ~fromStore=store => store.items,
    ~select=items => items,
    (),
  );
};
```

### Optimistic Updates

```reason
let addItemOptimistic = (item: Item.t) => {
  let update = Store.useUpdate();
  let revertTimeout = React.useRef(None);
  
  // Optimistically add item
  update(store => {
    ...store,
    items: [item, ...store.items],
  });
  
  // API call
  Api.addItem(item)
  |> Js.Promise.then_(() => {
    // Success - keep the item
    revertTimeout.current->Belt.Option.forEach(Js.Global.clearTimeout);
    Js.Promise.resolve();
  })
  |> Js.Promise.catch(_ => {
    // Failure - revert after delay
    revertTimeout.current = Some(
      Js.Global.setTimeout(() => {
        update(store => {
          ...store,
          items: store.items |> List.filter(i => i.id != item.id),
        });
      }, 3000)
    );
    Js.Promise.resolve();
  });
};
```

### Store Composition

```reason
// Combine multiple stores
type combinedStore = {
  user: UserStore.t,
  cart: CartStore.t,
  settings: SettingsStore.t,
};

let combinedStore = {
  user: UserStore.useStore(),
  cart: CartStore.useStore(),
  settings: SettingsStore.useStore(),
};
```

### Real-time Sync Configuration

```reason
// Enable real-time updates for specific data
let subscriptionOfConfig = (config: config): option(subscription) =>
  switch (config.user) {
  | Some(user) => Some({userId: user.id})
  | None => None
  };

// Configure patch decoding
let decodePatch =
  StorePatch.compose([
    // Table: items
    StorePatch.Pg.decodeAs(
      ~table="items",
      ~decodeRow=Item.of_json,
      ~insert=data => ItemAdd(data),
      ~update=data => ItemUpdate(data.id, data),
      ~delete=id => ItemDelete(id),
      (),
    ),
    // Table: users
    StorePatch.Pg.decodeAs(
      ~table="users",
      ~decodeRow=User.of_json,
      ~insert=data => UserUpdate(data),
      ~update=data => UserUpdate(data),
      ~delete=_id => UserRemove,
      (),
    ),
  ]);
```

## Server-Side Integration

### Fetching Initial State

```reason
// EntryServer.re
let getServerState = (context: UniversalRouterDream.serverContext) => {
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let request = UniversalRouterDream.contextRequest(context);
  
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
  type t = cart;
  let key = "cart_v1";  // Version your keys!
  let default = () => {items: []};
  let encode = cart_to_json;
  let decode = cart_of_json;
});
```

## Best Practices

### 1. Keep Source State Serializable

✅ **Good:**
```reason
type config = {
  items: list(Item.t),
  userId: string,
};
```

❌ **Bad:**
```reason
type config = {
  items: array(Item.t),  // Arrays don't serialize well
  ref: React.ref(option(Dom.element)),  // Can't serialize refs
};
```

### 2. Normalize Complex Data

✅ **Good:**
```reason
type config = {
  users: StringMap.t(User.t),
  posts: StringMap.t(Post.t),
  currentUserId: string,
};
```

❌ **Bad:**
```reason
type config = {
  posts: list({
    ...postFields,
    author: User.t,  // Nested data duplicates user data
    comments: list(Comment.t),  // Deep nesting
  }),
};
```

### 3. Use Projections for Derived Data

✅ **Good:**
```reason
let itemCount =
  StoreBuilder.projected(
    ~project=items => List.length(items),
    ~serverSource=store.items,
    ~fromStore=store => store.items,
    ~select=count => count,
    (),
  );
```

❌ **Bad:**
```reason
// Computing in every render
let itemCount = List.length(store.items);
```

### 4. Version Your Persisted Stores

```reason
let key = "app_state_v2";  // Increment when schema changes
```

### 5. Handle Schema Migrations

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
- Product catalog
- Shopping cart with persistence
- User authentication
- Real-time inventory updates

### Todo App Store

```reason
// TodoStore.re
[@deriving json]
type todo = {
  id: string,
  text: string,
  completed: bool,
  createdAt: Js.Date.t,
};

type filter =
  | All
  | Active
  | Completed;

type state = {
  todos: list(todo),
  filter: filter,
};

type patch =
  | TodoAdd(todo)
  | TodoToggle(string)
  | TodoDelete(string)
  | TodoEdit(string, string);

// ... implementation
```

## Related Documentation

- [Dream Router Store Setup](dream-router-store-setup.md) - Complete integration example
- [Real-time Middleware](reason-realtime.dream-middleware.md) - WebSocket setup
- [PostgreSQL Adapter](reason-realtime.pgnotify-adapter.md) - Database notifications
- [Tilia Documentation](https://github.com/darklang/tilia) - Underlying state management
