# universal-reason-react/store

> ⚠️ **API Stability**: APIs are not stable and are subject to change.

Tilia-backed store tooling for universal Reason React applications with synchronous SSR hydration, IndexedDB persistence, and optional realtime sync.

## Overview

The current store model is runtime-first:

- `StoreBuilder.Runtime.Make` builds a local-only runtime store
- `StoreBuilder.Runtime.MakeSynced` builds a realtime runtime store
- SSR always hydrates synchronously from the server-rendered payload
- IndexedDB is the browser persistence layer
- local-only stores persist confirmed snapshots to IndexedDB and sync across tabs with `BroadcastChannel`
- synced stores persist confirmed snapshots and an action ledger in IndexedDB, then reconcile with websocket acks, patches, and snapshots

## Builder Choice

### `StoreBuilder.Runtime.Make`

Use this for local-only state such as the simple todo demo or the ecommerce cart.

Required schema fields:

- `type state`
- `type action`
- `type store`
- `reduce`
- `emptyState`
- `storeName`
- `stateElementId`
- `scopeKeyOfState`
- `timestampOfState`
- `state_of_json`
- `state_to_json`
- `action_of_json`
- `action_to_json`
- `makeStore`

Behavior:

- hydrates synchronously from SSR
- loads `confirmed_state` from IndexedDB after mount
- persists confirmed state back to IndexedDB when SSR wins or state changes
- broadcasts newer confirmed state across tabs for the same `storeName`
- does not create an action ledger

Example:

```reason
module Runtime = StoreBuilder.Runtime.Make({
  type nonrec state = state;
  type nonrec action = action;
  type nonrec store = store;

  let reduce = reduce;
  let emptyState = emptyState;
  let storeName = "todo.simple";
  let stateElementId = "initial-store";
  let scopeKeyOfState = (_state: state) => "default";
  let timestampOfState = (state: state) => state.updated_at;
  let state_of_json = state_of_json;
  let state_to_json = state_to_json;
  let action_of_json = action_of_json;
  let action_to_json = action_to_json;
  let makeStore = (~state, ~derive=?, ()) => {
    state:
      StoreBuilder.current(
        ~derive?,
        ~client=state,
        ~server=() => state,
        (),
      ),
    total_count:
      StoreBuilder.derived(
        ~derive?,
        ~client=store => Array.length(store.state.todos),
        ~server=() => Array.length(state.todos),
        (),
      ),
  };
});
```

### `StoreBuilder.Runtime.MakeSynced`

Use this for realtime state such as `todo-multiplayer` or ecommerce inventory.

Additional schema fields:

- `type subscription`
- `type patch`
- `setTimestamp`
- `subscriptionOfState`
- `encodeSubscription`
- `eventUrl`
- `baseUrl`
- `decodePatch`
- `updateOfPatch`

Behavior:

- hydrates synchronously from SSR
- reconciles the newer confirmed snapshot from IndexedDB after mount
- persists the confirmed snapshot into IndexedDB even when SSR wins
- queues optimistic actions in IndexedDB
- sends JSON mutation envelopes over the active websocket
- retries with the same `actionId` until ack or retry exhaustion
- rebuilds optimistic state from confirmed state plus remaining pending actions

Example:

```reason
module Runtime = StoreBuilder.Runtime.MakeSynced({
  type nonrec state = state;
  type nonrec action = action;
  type nonrec store = store;
  type nonrec subscription = subscription;
  type nonrec patch = patch;

  let reduce = reduce;
  let emptyState = emptyState;
  let storeName = "todo-multiplayer";
  let stateElementId = "initial-store";
  let scopeKeyOfState = scopeKeyOfState;
  let timestampOfState = timestampOfState;
  let setTimestamp = setTimestamp;
  let state_of_json = state_of_json;
  let state_to_json = state_to_json;
  let action_of_json = action_of_json;
  let action_to_json = action_to_json;
  let makeStore = (~state, ~derive=?, ()) => {
    state,
    total_count:
      StoreBuilder.derived(
        ~derive?,
        ~client=store => Array.length(store.state.todos),
        ~server=() => Array.length(state.todos),
        (),
      ),
  };
  let subscriptionOfState = subscriptionOfState;
  let encodeSubscription = RealtimeSubscription.encode;
  let eventUrl = Constants.event_url;
  let baseUrl = Constants.base_url;
  let decodePatch = decodePatch;
  let updateOfPatch = updateOfPatch;
});
```

## IndexedDB Layout

Each runtime store uses one IndexedDB database per `storeName`.

Object stores:

- `confirmed_state`
  - key: `scopeKey`
  - value: `{scopeKey, value, timestamp}`
- `actions`
  - synced runtimes only
  - key: `id`
  - value: `{id, scopeKey, action, status, enqueuedAt, retryCount, error}`

`scopeKey` should be:

- `"default"` for global stores
- a route or subscription identity for scoped stores, such as a todo list id or premise id

## Hydration and Reconcile

All runtimes follow the same startup model:

1. server renders the initial state into the document
2. client calls `hydrateStore()` synchronously
3. runtime mounts a captured `StoreSource`
4. runtime reads IndexedDB in the background
5. runtime chooses the newer confirmed state by `timestampOfState`
6. runtime applies any additional behavior:
   - local-only: publish newer confirmed state across tabs
   - synced: replay pending actions on top of confirmed state

This means SSR stays server-first and there is no promise-gated boot path.

## Dispatch Model

### Local-only runtime

- `dispatch(action)` reduces immediately
- the resulting state is the new confirmed state
- the confirmed state is written to IndexedDB
- a broadcast message is sent to sibling tabs for the same store

### Synced runtime

- `dispatch(action)` reduces optimistically
- the typed action is serialized and stored in the IndexedDB action ledger
- the runtime sends `{type: "mutation", actionId, action}` over the websocket
- server responds with `{type: "ack", actionId, status, error?}`
- snapshots and patches advance confirmed state
- optimistic state is rebuilt from confirmed state plus remaining pending actions

## Realtime Patch Helpers

Use `StoreCrud` for standard table-backed patch handling.

```reason
type patch = StoreCrud.patch(MyRow.t);

let decodePatch =
  StorePatch.compose([
    StoreCrud.decodePatch(
      ~table=RealtimeSchema.table_name("items"),
      ~decodeRow=MyRow.of_json,
      (),
    ),
  ]);

let updateOfPatch =
  StoreCrud.updateOfPatch(
    ~getId=(item: MyRow.t) => item.id,
    ~getItems=(state: state) => state.items,
    ~setItems=(state: state, items) => {...state, items},
  );
```

## React Usage

The generated runtime module exposes `Context` in addition to the builder exports.

```reason
let store = Runtime.hydrateStore();

<Runtime.Context.Provider value=store>
  <App />
</Runtime.Context.Provider>
```

In components:

```reason
let store = Runtime.Context.useStore();
```

## Troubleshooting

### Hydration mismatch

- keep `StoreBuilder.current` client and server branches compatible
- avoid non-deterministic values during SSR
- make sure `timestampOfState` comes from state, not a fresh `Date.now()` during render

### IndexedDB does not win when expected

- verify `timestampOfState` is monotonic for the store
- verify `scopeKeyOfState` matches the intended route or subscription identity
- check browser devtools for the `confirmed_state` record under the store’s database name

### Synced actions replay incorrectly

- ensure `action_of_json` and `action_to_json` are symmetric
- prefer explicit state-setting actions over toggles so retries stay idempotent
- confirm the server sends `ack`, `patch`, and `snapshot` JSON frames with the expected shape

## Demo References

- local-only runtime: `demos/todo/ui/src/TodoStore.re`
- local-only runtime with IndexedDB cart persistence: `demos/ecommerce/ui/src/CartStore.re`
- synced runtime: `demos/todo-multiplayer/ui/src/TodoStore.re`
- synced runtime with projected values: `demos/ecommerce/ui/src/Store.re`
