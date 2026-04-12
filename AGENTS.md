# AGENTS.md

Instructions for AI agents working on this codebase.

## Project Overview

Resync is a framework for building realtime universal (SSR + client) Reason React applications with PostgreSQL-backed state. The monorepo contains reusable packages and demo apps.

## Build & Run

```bash
# Install dependencies
pnpm install

# Build everything (continuous watch)
pnpm run dune:watch

# Run ecommerce demo (requires separate terminal for each)
pnpm run ecommerce:watch    # restarts server on UI rebuild

# Run todo demo
pnpm run todo:watch         # restarts server on UI rebuild

# Build a specific target
dune build @app                                    # ecommerce client (from demos/ecommerce)
dune build demos/ecommerce/server/src/server.exe   # ecommerce server (from repo root)
dune build demos/todo/ui/src/.build_stamp          # todo client (from repo root)
dune build demos/todo/server/src/server.exe        # todo server (from repo root)
```

### Running dune commands

**For AI agents:** Due to orchestration tool issues with running `dune` directly (stdout/stderr capture, timeout handling), agents should prefer using the Python wrapper:

```bash
python scripts/run_dune.py build @all-apps
python scripts/run_dune.py build packages/universal-reason-react/store/js
python scripts/run_dune.py exec ./packages/universal-reason-react/store/test/store_runtime_test.exe
python scripts/run_dune.py clean
```

The wrapper runs `dune` from the repo root, captures output cleanly, and has a configurable timeout (default 120s, override with `DUNE_TIMEOUT` env var). End users should run `dune` directly.

### Build verification

After making changes, always rebuild to verify:
1. `dune build @app` from `demos/ecommerce` (or equivalent for the target you changed)
2. `dune build demos/<demo>/server/src/server.exe` from repo root

### Environment variables

Each demo requires specific env vars in `.envrc`:
- **Ecommerce**: `DB_URL`, `API_BASE_URL`, `ECOMMERCE_DOC_ROOT`
- **Todo**: `TODO_DOC_ROOT` (set to `./_build/default/demos/todo/ui/src/`)
- **Todo Multiplayer**: `DB_URL`, `TODO_MP_DOC_ROOT` (set to `./_build/default/demos/todo-multiplayer/ui/src/`)

### Docker

Ecommerce demo uses Docker for PostgreSQL:
```bash
docker compose up -d    # starts postgres with auto-initialized schema + triggers
```

## Testing

Use Playwright MCP for end-to-end browser testing. Do NOT create Node.js test scripts.

Browser test scripts compile with Melange and run with Node.js:

```bash
# Video chat demo browser tests
pnpm run video-chat:test:browser
```

### LSP diagnostics on Reason/OCaml files

The `lsp_diagnostics` tool has limited support for Reason (`.re`) and OCaml (`.ml`) files in this project, especially in test directories. It commonly returns:

- `No supported source files found in directory` when scanning directories containing `.re` or `.ml` files
- `No diagnostics found` for individual files even when build errors exist

**Workaround:** rely on `dune build` (via the Python wrapper) as the source of truth for compile errors and type issues, rather than LSP diagnostics.


## Project Structure

```
packages/
  realtime-schema/          # SQL annotation parser, codegen, PPX, CLI
    src/
      Realtime_schema_types.ml
      Realtime_schema_parser.ml
      Realtime_schema_codegen.ml
      Realtime_schema_ppx.ml
      Realtime_schema_caqti.ml
  universal-reason-react/
    router/                 # Universal router (shared route tree for server + client)
    store/                  # Tilia-backed offline-first store with SSR, persistence, realtime sync
      js/                   # JS/Melange implementations
        StoreBuilder.re     # Public runtime store builders
        StoreCrud.re        # Generic CRUD helpers for realtime patches
        StoreIndexedDB.re   # IndexedDB confirmed-state and action-ledger storage
        StoreOffline.re     # Local and synced runtime implementations
        StorePatch.re       # Patch decoding infrastructure
        StoreSource.re      # Tilia source wrappers
      native/               # Server-side copies
  reason-realtime/
    pgnotify-adapter/       # PostgreSQL LISTEN/NOTIFY adapter
    dream-middleware/       # Dream websocket middleware
  esbuild-plugin/           # Client component extraction
  universal-reason-react/   # Also: components, lucide-icons, intl

demos/
  ecommerce/                # Full demo: DB, realtime, SSR
    server/sql/             # Annotated SQL files (source of truth)
      generated/            # Auto-generated triggers, migrations, snapshots
    server/src/             # Dream server
    ui/src/                 # Client UI (Reason React)
    shared/js/              # Shared types (Model.re, RealtimeSchema.ml)
    shared/native/          # Server-side shared types
  todo/                     # Minimal demo: SSR, hydration, no DB/realtime
```

## Universal Rendering with server-reason-react

This project uses **server-reason-react** to render the same ReasonReact code on both the server (native OCaml) and the client (JavaScript via Melange). This is not an isomorphic Node.js setup — the server is a compiled native binary.

### How it works

- **Melange** compiles `.re` files to `.js` in a `(melange.emit ...)` target directory (e.g., `app/`)
- **esbuild** bundles the Melange JS output into `Index.re.js` and `Index.re.css` for the browser
- The **server** compiles the same `.re` files as native OCaml via `server-reason-react`'s PPX transforms
- The server's dune file copies `.re` sources from `ui/src/` into the server build context using `(copy_files)`
- Shared types live in `shared/js/` (Melange) and `shared/native/` (native), each with their own dune library

### Dual compilation constraints

Code that runs on both server and client must compile under both targets. This means:

- **Use `Js.Array.*` for array operations** — these are polyfilled by server-reason-react for native. Do NOT use `Array.append` (OCaml stdlib) or `List.*` for store collections.
- **Use `->` chaining for `Js.Array.*` methods** — they use `[@mel.this]` binding: `items->Js.Array.filter(~f=...)`
- **Use `switch%platform`** for platform-specific behavior (DOM access, localStorage, etc.)
- **Use `[@platform js]` / `[@platform native]`** for platform-specific module implementations (provided by server-reason-react PPX)
- **Do not use browser-only APIs** outside of `[@platform js]` blocks (e.g., `document`, `window`, `fetch`)
- **Do not use Node.js APIs or npm packages** — the server is native OCaml, not Node

### Dune library structure

Each package that needs to work on both targets has two dune libraries:
```
shared/
  js/dune       # (library ...) with melange PPX preprocessors
  native/dune   # (library ...) with native PPX preprocessors
```

The server depends on `*_native` libraries; the client depends on `*_js` libraries. Both compile the same `.re` source files.

## Code Conventions

### Language

- Reason (`*.re`) for application code and store/router libraries
- OCaml (`*.ml`) for infrastructure (PPX, codegen, CLI, native adapters)
- SQL (`*.sql`) with comment annotations for schema definitions

### Store pattern

All stores use the terminal value-level builders (`StoreBuilder.buildLocal`, `StoreBuilder.buildSynced`, or `StoreBuilder.buildCrud`) with the pipeline API. Do NOT define top-level bindings and then pass them through as `let x = x`:

```reason
// CORRECT: terminal builder with inline pipeline
module StoreDef =
  (val StoreBuilder.buildLocal(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         storeName: "todo.simple",
         emptyState: { /* ... */ },
         reduce: (~state, ~action) => state,
         makeStore: (~state, ~derive=?, ()) => { /* ... */ },
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

// WRONG: top-level bindings passed to functor
let emptyState = { /* ... */ };
let reduce = (~state, ~action) => state;
module Runtime = StoreBuilder.Runtime.Make({
  let emptyState = emptyState;
  let reduce = reduce;
});
```

Runtime behavior:
- `StoreBuilder.buildLocal` is local-only and persists confirmed snapshots to IndexedDB, then propagates newer confirmed state across tabs with `BroadcastChannel`
- `StoreBuilder.buildSynced` and `StoreBuilder.buildCrud` persist confirmed snapshots plus an IndexedDB action ledger, send typed JSON actions over websocket, and propagate optimistic actions plus confirmed snapshots across tabs
- For synced stores, cross-tab updates should not depend solely on websocket delivery; optimistic replay comes from the shared IndexedDB ledger

### Collections use `array`, not `list`

Store state uses `array` throughout. Use `Js.Array.*` functions (which work in both Melange JS and native via server-reason-react):
- `Js.Array.filter(~f=..., items)` — not `List.filter`
- `Js.Array.map(~f=..., items)` — not `List.map`
- `Js.Array.concat(~other=..., items)` — not `Array.append` or `List.append`
- Use `->` chaining for `Js.Array.*` methods (they use `[@mel.this]`): `items->Js.Array.filter(~f=...)`
- `Array.length` is fine for getting length

### Patch handling with StoreCrud

Use `StoreCrud` for standard CRUD tables. Do NOT write custom patch types, upsert/delete functions, or manual `StorePatch.Pg.decodeAs` wiring:

```reason
type patch = StoreCrud.patch(MyItem.t);

// Inside functor body:
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

For multi-table stores, compose decoders and use a wrapped variant:
```reason
type patch =
  | ItemsPatch(StoreCrud.patch(Item.t))
  | UsersPatch(StoreCrud.patch(User.t));
```

### SQL-first schema

Annotated SQL files in `server/sql/` are the source of truth. The PPX reads them at compile time.

Key annotations:
- `-- @table <name>` — marks a table for realtime
- `-- @id_column <col>` — primary key column
- `-- @broadcast_channel column=<col>` — which column determines the NOTIFY channel
- `-- @broadcast_parent table=<parent> query=<named_query>` — child table triggers parent re-broadcast
- `-- @composite_key <col1>, <col2>` — composite primary key
- `-- @query <name>` — named query in block comments (`/* ... */`)
- `-- @json_column <col>` — column returned as `::text` that needs JSON normalization in triggers

### Don't modify files the user didn't ask about

Especially `package.json`, watch scripts, or unrelated config files.

## Key Documentation

- `REALTIME_QUERY_REFACTOR.md` — full architecture plan for the realtime schema system
- `docs/universal-reason-react.store.md` — store API reference and patterns
- `docs/dream-router-store-setup.md` — step-by-step guide for Dream + Router + Store setup
- `docs/API_REFERENCE.md` — complete API reference for all packages
- `docs/reason-realtime.pgnotify-adapter.md` — PostgreSQL adapter docs
- `docs/reason-realtime.dream-middleware.md` — Dream websocket middleware docs
