# Getting Started with Resync

> Build a PostgreSQL-backed realtime app with synced store, queries, and mutations in under 30 minutes.

This guide walks through creating a minimal collaborative todo list that syncs across browsers via PostgreSQL. You'll learn:

- SQL schema with realtime annotations
- Dream server with WebSocket middleware
- Synced CRUD store with optimistic updates
- Universal routing for SSR + hydration

## Prerequisites

- **PostgreSQL** 14+ (via Docker or local install)
- **opam** with OCaml 4.14+
- **pnpm** 8+
- **dune** 3.8+

Install dependencies from repo root:

```bash
pnpm install
opam install . --deps-only
```

## What You'll Build

A single-page todo list where:
- Multiple users can collaborate in real-time
- Changes sync instantly across browser tabs
- Offline changes persist and reconcile when reconnected
- Server renders initial HTML for fast page load

## Project Structure

```
my-app/
├── server/
│   ├── sql/
│   │   └── schema.sql          # Annotated SQL schema
│   └── src/
│       ├── server.ml           # Dream server entrypoint
│       └── EntryServer.re      # SSR state + rendering
├── ui/
│   └── src/
│       ├── Index.re            # Client hydration entrypoint
│       ├── Routes.re           # Universal router config
│       └── TodoStore.re        # Synced CRUD store
└── shared/
    ├── js/
    │   ├── Model.re            # Shared types (Melange)
    │   ├── Constants.re        # URLs/config
    │   └── RealtimeSchema.ml   # Schema metadata (PPX)
    └── native/
        ├── dune                # Native library config
        └── RealtimeSchema.ml   # Schema metadata (native)
```

---

## Step 1: SQL Schema with Annotations

Create `server/sql/schema.sql` with realtime annotations:

```sql
-- @table todo_lists
-- @id_column id
-- @broadcast_channel column=id

CREATE TABLE todo_lists (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  name text NOT NULL DEFAULT 'My Todo List',
  created_at timestamptz NOT NULL DEFAULT NOW(),
  updated_at timestamptz NOT NULL DEFAULT NOW()
);

-- @table todos
-- @id_column id
-- @broadcast_channel column=list_id
-- @broadcast_parent table=todo_lists query=get_list

CREATE TABLE todos (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  list_id uuid NOT NULL REFERENCES todo_lists(id) ON DELETE CASCADE,
  text text NOT NULL,
  completed boolean NOT NULL DEFAULT false,
  created_at timestamptz NOT NULL DEFAULT NOW()
);

/*
@query get_list
SELECT id, list_id, text, completed FROM todos WHERE list_id = $1 ORDER BY created_at;
*/

/*
@mutation add_todo
INSERT INTO todos (id, list_id, text)
VALUES ($1::uuid, $2::uuid, $3);
*/
```

### Key Annotations

| Annotation | Purpose |
|------------|---------|
| `-- @table <name>` | Marks table for realtime tracking |
| `-- @id_column <col>` | Primary key column for patches |
| `-- @broadcast_channel column=<col>` | Determines NOTIFY channel |
| `-- @broadcast_parent table=<t> query=<q>` | Propagates updates to parent |
| `/* @query <name> */` | Named query for server-side use |
| `/* @mutation <name> */` | Named mutation (idempotency handled by middleware) |

### Mutation Pattern

Mutations should only contain the logical data change. The middleware automatically ensures idempotency by tracking each `action_id` in a per-mutation system table (`_resync_actions_<name>`).

---

## Step 2: Generate Realtime Artifacts

The PPX reads your SQL at compile time and generates:

- PostgreSQL triggers for NOTIFY
- Migration SQL for schema setup
- Snapshot queries
- OCaml module with table names and column metadata

Run code generation:

```bash
# From repo root
python scripts/run_dune.py build @all-apps
```

This generates `server/sql/generated/realtime.sql` containing triggers.

Initialize your database:

```bash
# Using Docker (recommended)
docker compose up -d

# Or apply manually
psql $DB_URL -f server/sql/schema.sql
psql $DB_URL -f server/sql/generated/realtime.sql
```

---

## Step 3: Shared Types

### Model.re (shared/js/Model.re)

Define types that work in both Melange (JS) and native (server):

```reason
open Melange_json.Primitives;

module Todo = {
  [@deriving json]
  type t = {
    id: string,
    list_id: string,
    text: string,
    completed: bool,
  };
};

module TodoList = {
  [@deriving json]
  type t = {
    id: string,
    name: string,
    updated_at: float,
  };
};

[@deriving json]
type t = {
  todos: array(Todo.t),
  list: option(TodoList.t),
};
```

### Constants.re (shared/js/Constants.re)

```reason
let event_url = "/_events";
let base_url = "http://localhost:8080";
```

### RealtimeSchema.ml (shared/js/ and shared/native/)

Both versions contain the same PPX invocation:

```ocaml
(* SQL-first schema metadata entrypoint *)
[%%realtime_schema "my-app/server/sql"]
```

---

## Step 4: Dune Configuration

### Shared JS Library (shared/js/dune)

```lisp
(library
 (name my_app_js)
 (public_name resync.my_app_js)
 (modules Model Constants RealtimeSchema)
 (wrapped false)
 (libraries
  melange-json
  universal_reason_react_store_js)
 (modes melange)
 (preprocess
  (pps
   realtime_schema_ppx
   server-reason-react.browser_ppx -js
   melange.ppx
   melange-json.ppx)))
```

### Shared Native Library (shared/native/dune)

```lisp
(library
 (name my_app_native)
 (public_name resync.my_app_native)
 (modules Model Constants RealtimeSchema)
 (wrapped false)
 (libraries
  melange-json-native
  server-reason-react.js
  universal_reason_react_store_native
  str)
 (preprocess
  (pps
   realtime_schema_ppx
   server-reason-react.browser_ppx
   server-reason-react.melange_ppx
   melange-json-native.ppx))
 (modes native))

(copy_files
 (files ../js/*.re))
```

---

## Step 5: Synced CRUD Store

Create `ui/src/TodoStore.re`:

```reason
open Melange_json.Primitives;

[@deriving json]
type state = Model.t;

type subscription = RealtimeSubscription.t;

type add_todo_payload = {
  id: string,
  list_id: string,
  text: string,
};

type action =
  | AddTodo(add_todo_payload)
  | SetTodoCompleted({id: string, completed: bool})
  | RemoveTodo(string);

type store = {
  list_id: string,
  state: state,
  completed_count: int,
  total_count: int,
};

let emptyState: state = {
  todos: [||],
  list: None,
};

let scopeKeyOfState = (state: state) =>
  switch (state.list) {
  | Some(list) => list.id
  | None => "default"
  };

let timestampOfState = (state: state) =>
  switch (state.list) {
  | Some(list) => list.updated_at
  | None => 0.0
  };

let setTimestamp = (~state, ~timestamp) =>
  switch (state.list) {
  | Some(list) => {...state, list: Some({...list, updated_at: timestamp})}
  | None => state
  };

/* Action serialization for websocket transport */
let action_to_json = action =>
  switch (action) {
  | AddTodo(payload) =>
    StoreJson.parse(
      {j|{"kind":"add_todo","payload":{"id":"|j}
      ++ payload.id
      ++ {j|","list_id":"|j}
      ++ payload.list_id
      ++ {j|","text":"|j}
      ++ payload.text
      ++ {j|"}}|j}
    )
  | SetTodoCompleted(payload) =>
    StoreJson.parse(
      {j|{"kind":"set_todo_completed","payload":{"id":"|j}
      ++ payload.id
      ++ {j|","completed":|j}
      ++ string_of_bool(payload.completed)
      ++ {j|}}|j}
    )
  | RemoveTodo(id) =>
    StoreJson.parse({j|{"kind":"remove_todo","payload":{"id":"|j} ++ id ++ {j|"}}|j})
  };

let action_of_json = json => {
  let kind = StoreJson.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
  let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
  switch (kind) {
  | "add_todo" =>
    AddTodo({
      id: StoreJson.requiredField(~json=payload, ~fieldName="id", ~decode=string_of_json),
      list_id: StoreJson.requiredField(~json=payload, ~fieldName="list_id", ~decode=string_of_json),
      text: StoreJson.requiredField(~json=payload, ~fieldName="text", ~decode=string_of_json),
    })
  | "set_todo_completed" =>
    SetTodoCompleted({
      id: StoreJson.requiredField(~json=payload, ~fieldName="id", ~decode=string_of_json),
      completed: StoreJson.requiredField(~json=payload, ~fieldName="completed", ~decode=bool_of_json),
    })
  | "remove_todo" =>
    RemoveTodo(StoreJson.requiredField(~json=payload, ~fieldName="id", ~decode=string_of_json))
  | _ => failwith("Unknown action kind")
  };
};

/* Optimistic reducer */
let reduce = (~state, ~action) => {
  let updatedAt = Js.Date.now();
  let withTimestamp = nextState => setTimestamp(~state=nextState, ~timestamp=updatedAt);
  switch (action) {
  | AddTodo(payload) =>
    withTimestamp({
      ...state,
      todos: StoreCrud.upsert(
        ~getId=(item: Model.Todo.t) => item.id,
        state.todos,
        {id: payload.id, list_id: payload.list_id, text: payload.text, completed: false}
      ),
    })
  | SetTodoCompleted(payload) =>
    withTimestamp({
      ...state,
      todos: Js.Array.map(
        ~f=(item: Model.Todo.t) =>
          item.id == payload.id ? {...item, completed: payload.completed} : item,
        state.todos
      ),
    })
  | RemoveTodo(id) =>
    withTimestamp({
      ...state,
      todos: StoreCrud.remove(~getId=(item: Model.Todo.t) => item.id, state.todos, id),
    })
  };
};

/* Build the synced CRUD store */
module StoreDef =
  (val StoreBuilder.buildCrud(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
      emptyState,
      reduce,
      makeStore: (~state, ~derive=?, ()) => {
        {
          list_id: switch (state.list) {| Some(list) => list.id | None => ""},
          state,
          completed_count: StoreBuilder.Crud.filteredCount(
            ~derive?,
            ~getItems=(store: store) => store.state.todos,
            ~predicate=(item: Model.Todo.t) => item.completed,
            ()
          ),
          total_count: StoreBuilder.Crud.totalCount(
            ~derive?,
            ~getItems=(store: store) => store.state.todos,
            ()
          ),
        };
      },
    })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withSyncCrud(
      ~storeName="my-app",
      ~scopeKeyOfState,
      ~timestampOfState,
      ~setTimestamp,
      ~transport={
        subscriptionOfState: (state: state): option(subscription) =>
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
      ~setItems=(state, items) => {...state, todos: items},
      ~hooks={
        StoreBuilder.Sync.onActionError: Some(message => Js.log("Action error: " ++ message)),
        onActionAck: None,
        onCustom: None,
        onMedia: None,
        onError: None,
        onOpen: None,
        onConnectionHandle: None,
      },
      ~stateElementId=Some("initial-store"),
      ()
    ),
  ));

/* Re-export public interface */
include (
  StoreDef:
    StoreBuilder.Runtime.Exports
    with type state := state
    and type action := action
    and type t := store
);

type t = store;
module Context = StoreDef.Context;

/* Action dispatchers */
let addTodo = (store: t, text: string) => {
  let list_id = switch (store.state.list) {
  | Some(list) => list.id
  | None => store.list_id
  };
  dispatch(AddTodo({id: UUID.make(), list_id, text}));
};

let toggleTodo = (store: t, id: string) =>
  switch (Js.Array.find(~f=(todo: Model.Todo.t) => todo.id == id, store.state.todos)) {
  | Some(todo) => dispatch(SetTodoCompleted({id, completed: !todo.completed}))
  | None => ()
  };

let removeTodo = (_store: t, id: string) => dispatch(RemoveTodo(id));
```

### Key Points

1. **`buildCrud`** creates a synced store with automatic patch handling
2. **`StoreCrud.upsert`/`remove`** update arrays optimistically
3. **`withSyncCrud`** configures websocket transport and table mapping
4. **`StoreBuilder.Crud.totalCount`/`filteredCount`** compute derived state

---

## Step 6: Server Setup

### server.ml (server/src/server.ml)

```ocaml
open Lwt.Syntax
open Mutation_result

let doc_root =
  match Sys.getenv_opt "MY_APP_DOC_ROOT" with
  | Some doc_root -> doc_root
  | None -> failwith "MY_APP_DOC_ROOT is required"

let db_uri =
  match Sys.getenv_opt "DB_URL" with
  | Some uri -> uri
  | None -> failwith "DB_URL is required"

let resolve_subscription request selection =
  match RealtimeSubscription.decode_channel selection with
  | None -> Lwt.return_none
  | Some list_id ->
    let* list_info = Dream.sql request (Database.Todo.get_list_info list_id) in
    Lwt.return (Option.map (fun _ -> list_id) list_info)

let get_config_json request list_id =
  let* todos = Dream.sql request (Database.Todo.get_list list_id) in
  let config : Model.t = {todos; list = None} in
  Lwt.return (TodoStore.serializeSnapshot config)

let realtime_adapter =
  Adapter.pack
    (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
    (Pgnotify_adapter.create ~db_uri ())

let handle_mutation _broadcast_fn request ~db ~action_id ~mutation_name action =
  (* Parse action and execute mutation - see full example *)
  Lwt.return (Ack (Ok ()))

let realtime_middleware =
  Middleware.create
    ~adapter:realtime_adapter
    ~resolve_subscription
    ~load_snapshot:get_config_json
    ~handle_mutation
    ()

let () =
  (match Lwt_main.run (Adapter.start realtime_adapter) with
   | () -> ()
   | exception Failure msg ->
     Printf.eprintf "Failed to connect notification listener: %s\n" msg);
  Dream.run
  @@ Dream.logger
  @@ Dream.sql_pool ~size:10 db_uri
  @@ Dream.router [
    Middleware.route "_events" realtime_middleware;
    Dream.get "/static/**" (Dream.static doc_root);
    Dream.get "/app.js" (fun req -> Dream.from_filesystem doc_root "Index.re.js" req);
    Dream.get "/style.css" (fun req -> Dream.from_filesystem doc_root "Index.re.css" req);
    Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
  ]
```

### EntryServer.re (server/src/EntryServer.re)

```reason
open Lwt.Syntax;

let getServerState = (context: UniversalRouterDream.serverContext(TodoStore.t)) => {
  let UniversalRouterDream.{basePath, request} = context;
  /* Extract list_id from path, fetch from DB, return store */
  let* todos = Dream.sql(request, Database.Todo.get_list(listId));
  let config: Model.t = {todos, list: Some(list)};
  let store = TodoStore.createStore(config);
  Lwt.return(UniversalRouterDream.State(store));
};

let render = (~context, ~serverState: TodoStore.t, ()) => {
  let store = serverState;
  let serializedState = TodoStore.serializeState(serverState.state);
  let UniversalRouterDream.{basePath, pathname, search} = context;
  let app =
    <UniversalRouter
      router=Routes.router
      state=store
      basePath
      serverPathname=pathname
      serverSearch=search
    />;
  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~basePath,
      ~pathname,
      ~search,
      ~serializedState,
      ~state=store,
      (),
    );
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
```

---

## Step 7: Client Entry Point

### Index.re (ui/src/Index.re)

```reason
let rootElement = ReactDOM.querySelector("#root");

let%browser_only _ =
  switch (rootElement) {
  | Some(domNode) =>
    let store = TodoStore.hydrateStore();
    let result =
      StoreBuilder.Bootstrap.withHydratedProvider(
        ~hydrateStore=() => store,
        ~provider=TodoStore.Context.Provider.make,
        ~children=
          React.array([|
            <UniversalRouter key="router" router=Routes.router state=store />,
          |]),
      );
    ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
  | None => Js.log("No root element found")
  };
```

### Routes.re (ui/src/Routes.re)

```reason
module NotFoundPage = {
  let make = (~pathname as _, ()) =>
    <div> {React.string("404 - Page not found")} </div>;
};

module HomePageComponent = {
  let make = (~params, ~searchParams as _, ()) => {
    let store = TodoStore.useStore();
    <div>
      <h1> {React.string("My Todo List")} </h1>
      <ul>
        {React.array(
          Js.Array.map(
            ~f=(todo: Model.Todo.t) =>
              <li key=todo.id>
                <input
                  type_="checkbox"
                  checked=todo.completed
                  onChange={_ => TodoStore.toggleTodo(store, todo.id)}
                />
                {React.string(todo.text)}
                <button onClick={_ => TodoStore.removeTodo(store, todo.id)}>
                  {React.string("Delete")}
                </button>
              </li>,
            store.state.todos
          )
        )}
      </ul>
      <button onClick={_ => TodoStore.addTodo(store, "New todo")}>
        {React.string("Add Todo")}
      </button>
    </div>;
  };
};

let router: UniversalRouter.t(TodoStore.t) =
  UniversalRouter.create(
    ~document=UniversalRouter.document(
      ~title="My App",
      ~stylesheets=[|"/style.css"|],
      ~scripts=[|"/app.js"|],
      ()
    ),
    ~notFound=(module NotFoundPage),
    [
      UniversalRouter.index(
        ~id="home",
        ~page=(module HomePageComponent),
        (),
      ),
      UniversalRouter.route(
        ~id="list",
        ~path=":listId",
        ~page=(module HomePageComponent),
        [],
        (),
      ),
    ],
  );
```

---

## Step 8: Build & Run

### Environment Variables

Add to `.envrc`:

```bash
export DB_URL="postgres://user:password@localhost:5432/myapp"
export MY_APP_DOC_ROOT="./_build/default/my-app/ui/src/"
```

### Build Commands

```bash
# Build everything
python scripts/run_dune.py build @all-apps

# Build server executable
python scripts/run_dune.py build my-app/server/src/server.exe

# Watch mode (development)
pnpm run my-app:watch
```

### Run

```bash
# Start PostgreSQL
docker compose up -d

# Start server
python scripts/run_dune.py exec ./my-app/server/src/server.exe
```

Open http://localhost:8080 in your browser.

---

## Verification

### Test Realtime Sync

1. Open the app in two browser tabs
2. Add a todo in Tab 1
3. Verify it appears in Tab 2 within 1 second
4. Toggle completion in Tab 2
5. Verify Tab 1 updates

### Test Offline Resilience

1. Open the app
2. Stop the server
3. Add a todo (appears optimistically)
4. Restart the server
5. Verify the todo persists and syncs

---

## Troubleshooting

### "No supported source files found"

The LSP has limited Reason/OCaml support. Use `dune build` for compile errors:

```bash
python scripts/run_dune.py build @app 2>&1 | head -50
```

### "Module not found"

Check dune library names use underscores, not hyphens:

```lisp
; Correct
(libraries universal_reason_react_store_js)

; Wrong
(libraries universal-reason-react-store-js)
```

### WebSocket not connecting

1. Verify `DB_URL` is set
2. Check PostgreSQL triggers exist:
   ```sql
   SELECT * FROM pg_trigger WHERE tgname LIKE 'realtime_notify_%';
   ```
3. Verify middleware route: `Middleware.route "_events" realtime_middleware`

---

## Next Steps

- Read [API Reference](API_REFERENCE.md) for complete API docs
- See [dream-router-store-setup.md](dream-router-store-setup.md) for detailed integration
- Explore [demos/todo-multiplayer](../demos/todo-multiplayer/) for a complete example
- Learn about [store-consistency-model.md](store-consistency-model.md) for internals

---

## Related Documentation

- [SQL Annotations](realtime-schema.sql-annotations.md) - Full annotation reference
- [Queries](realtime-schema.queries.md) - Named query patterns
- [Mutations](realtime-schema.mutations.md) - Mutation handler patterns
- [Dream Middleware](reason-realtime.dream-middleware.md) - WebSocket protocol
- [PostgreSQL Adapter](reason-realtime.pgnotify-adapter.md) - LISTEN/NOTIFY details
