# universal-reason-react/store

> ⚠️ **API Stability**: APIs are not stable and are subject to change.

Tilia-backed store tooling for universal Reason React applications with synchronous SSR hydration, IndexedDB persistence, and optional realtime sync.

## Overview

The current store model is runtime-first:

- `StoreBuilder.buildLocal` builds a local-only runtime store
- `StoreBuilder.buildSynced` builds a realtime runtime store
- `StoreBuilder.buildCrud` builds a realtime runtime store with CRUD patches
- SSR always hydrates synchronously from the server-rendered payload
- IndexedDB is the browser persistence layer
- local-only stores persist confirmed snapshots to IndexedDB and sync across tabs with `BroadcastChannel`
- synced stores persist confirmed snapshots and an action ledger in IndexedDB, then reconcile with websocket acks, patches, and snapshots

## End-to-End Authoring Example

The smallest complete store in the repo is the todo demo. It shows the full path from types to reducer to store shape to component dispatch.

### 1) Define the store

`demos/todo/ui/src/TodoStore.re` keeps the whole authoring story in one place:

```reason
[@deriving json]
type todo = {id: string, text: string, completed: bool};

[@deriving json]
type state = {todos: array(todo), updated_at: float};

type action =
  | AddTodo(todo)
  | SetTodoCompleted({id: string, completed: bool})
  | RemoveTodo(string);

type store = {state: state, completed_count: int, total_count: int};

let emptyState: state = {todos: [||], updated_at: 0.0};

let completedCount = todos => todos->Js.Array.filter(~f=(item: todo) => item.completed)->Array.length;

let action_to_json = action => switch (action) {
| AddTodo(todo) => StoreJson.parse("{\"kind\":\"add_todo\",\"todo\":" ++ StoreJson.stringify(todo_to_json, todo) ++ "}")
| SetTodoCompleted(input) =>
  StoreJson.parse(
    "{\"kind\":\"set_todo_completed\",\"id\":"
    ++ string_to_json(input.id)->Melange_json.to_string
    ++ ",\"completed\":"
    ++ bool_to_json(input.completed)->Melange_json.to_string
    ++ "}",
  )
| RemoveTodo(id) =>
  StoreJson.parse(
    "{\"kind\":\"remove_todo\",\"id\":"
    ++ string_to_json(id)->Melange_json.to_string
    ++ "}",
  )
};

let action_of_json = json => {
  let kind = StoreJson.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
  switch (kind) {
  | "add_todo" => AddTodo(StoreJson.requiredField(~json, ~fieldName="todo", ~decode=todo_of_json))
  | "set_todo_completed" =>
    SetTodoCompleted({
      id: StoreJson.requiredField(~json, ~fieldName="id", ~decode=string_of_json),
      completed:
        StoreJson.requiredField(~json, ~fieldName="completed", ~decode=bool_of_json),
    })
  | _ => RemoveTodo(StoreJson.requiredField(~json, ~fieldName="id", ~decode=string_of_json))
  };
};

let reduce = (~state: state, ~action: action) => {
  let updated_at = Js.Date.now();
  switch (action) {
  | AddTodo(todo) => {
      todos: StoreCrud.upsert(~getId=(item: todo) => item.id, state.todos, todo),
      updated_at,
    }
  | SetTodoCompleted(input) => {
      todos:
        Js.Array.map(
          ~f=(item: todo) => item.id == input.id ? {...item, completed: input.completed} : item,
          state.todos,
        ),
      updated_at,
    }
  | RemoveTodo(id) => {
      todos: StoreCrud.remove(~getId=(item: todo) => item.id, state.todos, id),
      updated_at,
    }
  };
};

module StoreDef =
  (val StoreBuilder.buildLocal(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState,
         reduce,
         makeStore: (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
           state: StoreBuilder.current(~derive?, ~client=state, ~server=() => state, ()),
            completed_count:
             StoreBuilder.derived(
               ~derive?,
               ~client=store => completedCount(store.state.todos),
               ~server=() => completedCount(state.todos),
               (),
             ),
            total_count:
             StoreBuilder.derived(~derive?, ~client=store => Array.length(store.state.todos), ~server=() => Array.length(state.todos), ()),
         },
       })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withLocalPersistence(
         ~storeName="todo.simple",
         ~scopeKeyOfState=_state => "default",
         ~timestampOfState=state => state.updated_at,
         ~stateElementId=None,
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

### 2) Consume it from a component

`demos/todo/ui/src/HomePage.re` reads the store and dispatches actions directly:

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

If you want to see how this store plugs into Dream SSR and hydration, jump to `docs/dream-router-store-setup.md`.

## Schema and Reducer Layer

Every store starts with `StoreBuilder.withSchema({emptyState, reduce, makeStore})`.

- `emptyState` is the SSR/server fallback and the initial client fallback.
- `reduce` is the pure state transition function. Keep it deterministic so retries and optimistic replay stay correct.
- `makeStore` defines the public store shape you expose to components.

`makeStore` is where you assemble the state surface using the projection helpers below:

- `StoreBuilder.current` for passthrough values
- `StoreBuilder.derived` for computed values
- `StoreBuilder.projected` when you need to project one shape into another before selecting from it

The `~derive` argument lets Tilia attach reactivity on the client; on the server the `~server` branch is used to keep SSR deterministic.

## Projection Primitives

These are the building blocks used inside `makeStore`.

- `current(~client, ~server)` for raw values that should pass through unchanged.
- `derived(~client, ~server)` for computed values.
- `projected(~project, ~serverSource, ~fromStore, ~select)` for nested projections.

Use `current` for plain state fields, `derived` for counters and flags, and `projected` when the store shape is layered.

## Selector Helpers

`StoreBuilder.Selectors` and `StoreBuilder.Crud` are boilerplate reducers, not separate query engines. They just wrap the primitives above for common cases.

- `Selectors.passthrough` → plain field passthrough
- `Selectors.clientOnly` → client state with a server fallback
- `Selectors.arrayLength` / `Selectors.filteredCount` → counts from arrays
- `Selectors.field` / `Selectors.computed` → nested projection helpers
- `Crud.totalCount` / `Crud.filteredCount` → common CRUD-store counters

See `docs/API_REFERENCE.md` for the complete signatures.

## Bootstrap Helpers

`StoreBuilder.Bootstrap` removes repeated provider/hydration ceremony from app entrypoints.

- `withHydratedProvider` for one store
- `withHydratedProviders` for nested stores
- `withCreatedProvider` for server-side store creation

If you need the raw context provider, you can still use it directly, but `Bootstrap` is the default ergonomic path.

## Builder Choice

### `StoreBuilder.buildLocal`

Use this for local-only state such as the simple todo demo or the ecommerce cart.

Pipeline steps:

1. `StoreBuilder.make()` — start the pipeline
2. `StoreBuilder.withSchema({emptyState, reduce, makeStore})` — define the core schema
3. `StoreBuilder.withGuardTree(~guardTree)` — optional declarative validation
4. `StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)` — wire codecs
5. `StoreBuilder.withLocalPersistence(...)` — finalize with persistence config

Behavior:

- hydrates synchronously from SSR
- loads `confirmed_state` from IndexedDB after mount
- persists confirmed state back to IndexedDB when SSR wins or state changes
- broadcasts newer confirmed state across tabs for the same `storeName`
- does not create an action ledger

The local runtime is the right choice when reducer output is the source of truth and no websocket reconciliation is needed.

Example:

```reason
module StoreDef =
  (val StoreBuilder.buildLocal(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState: {todos: [||], updated_at: 0.0},
         reduce: (~state, ~action) => state,
         makeStore: (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
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
         },
       })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withLocalPersistence(
         ~storeName="todo.simple",
         ~scopeKeyOfState=_state => "default",
         ~timestampOfState=state => state.updated_at,
         ~stateElementId=None,
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

### `StoreBuilder.buildSynced`

Use this for realtime state such as `todo-multiplayer`, ecommerce inventory, or the LLM chat demo.

Additional pipeline steps (after `withJson`):

1. `StoreBuilder.withGuardTree(~guardTree)` — optional validation before sync wiring
2. `StoreBuilder.withSync(...)` — wire transport, patches, hooks, and optional streams

Behavior:

- hydrates synchronously from SSR
- reconciles the newer confirmed snapshot from IndexedDB after mount
- persists the confirmed snapshot into IndexedDB even when SSR wins
- queues optimistic actions in IndexedDB
- sends JSON mutation envelopes over the websocket
- retries with the same `actionId` until ack or retry exhaustion
- rebuilds optimistic state from confirmed state plus remaining pending actions

The synced runtime is the right choice when actions are optimistic and the server can later confirm or patch them.

Example:

```reason
module StoreDef =
  (val StoreBuilder.buildSynced(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState: {todos: [||], updated_at: 0.0},
         reduce,
         makeStore: (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
           state,
           total_count:
             StoreBuilder.derived(
               ~derive?,
               ~client=store => Array.length(store.state.todos),
               ~server=() => Array.length(state.todos),
               (),
             ),
         },
       })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withSync(
         ~storeName="todo-multiplayer",
         ~scopeKeyOfState,
         ~timestampOfState,
         ~setTimestamp,
         ~decodePatch,
         ~updateOfPatch,
         ~transport={
           subscriptionOfState,
           encodeSubscription: RealtimeSubscription.encode,
           eventUrl: Constants.event_url,
           baseUrl: Constants.base_url,
         },
         ~stateElementId=None,
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

### `StoreBuilder.buildCrud`

For stores using standard CRUD patches, use `buildCrud` which pre-wires the patch decoding:

```reason
module StoreDef =
  (val StoreBuilder.buildCrud(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState,
         reduce,
         makeStore,
       })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withSyncCrud(
         ~storeName="todo.crud",
         ~scopeKeyOfState,
         ~timestampOfState,
         ~setTimestamp,
         ~transport={
           subscriptionOfState,
           encodeSubscription: RealtimeSubscription.encode,
           eventUrl: Constants.event_url,
           baseUrl: Constants.base_url,
         },
         ~table=RealtimeSchema.table_name("todos"),
         ~decodeRow=TodoItem.of_json,
         ~getId=(item: TodoItem.t) => item.id,
         ~getItems=(state: state) => state.todos,
         ~setItems=(state: state, todos) => {...state, todos},
         ~stateElementId=None,
         (),
       )
  ));
```

Pipeline steps:

1. `StoreBuilder.make()`
2. `StoreBuilder.withSchema({emptyState, reduce, makeStore})`
3. `StoreBuilder.withGuardTree(~guardTree)` *(optional)*
4. `StoreBuilder.withJson(...)`
5. `StoreBuilder.withSyncCrud(...)`

Use this when patch payloads correspond to ordinary table row upserts/deletes and you want the package to derive the patch decoder for you.

## Guard Trees (Validation)

StoreBuilder supports declarative guard trees that can be shared between client and server. A guard tree branches on state predicates and decides whether an action is allowed or denied before `reduce` runs.

```reason
let guardTree =
  StoreBuilder.GuardTree.whenTrue(
    ~condition=(state: state) =>
      switch (state.current_thread_id) {
      | Some(_) => true
      | None => false
      },
    ~then_=StoreBuilder.GuardTree.acceptAll,
    ~else_=
      StoreBuilder.GuardTree.denyIf(
        ~predicate=(action: action) =>
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

Wire it into the pipeline with `withGuardTree`:

```reason
StoreBuilder.make()
|> StoreBuilder.withSchema({...})
|> StoreBuilder.withGuardTree(~guardTree)
|> StoreBuilder.withJson(...)
```

Guard trees are evaluated before `reduce` on the client, and can also be evaluated on the server inside `handle_mutation` (or via the realtime middleware `validate_mutation` hook) to reject invalid actions with an error ack.

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

Prefer `StoreBuilder.Bootstrap` in entrypoints:

```reason
let {store, element} =
  StoreBuilder.Bootstrap.withHydratedProvider(
    ~hydrateStore=StoreDef.hydrateStore,
    ~provider=StoreDef.Context.Provider,
    ~children=<App />,
  );
```

Use `StoreDef.Context.useStore()` inside components.

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
- synced runtime with CRUD patches: `demos/todo-multiplayer/ui/src/TodoStore.re`
- synced runtime with projected values: `demos/ecommerce/ui/src/Store.re`
