# resync

`resync` is an early alpha monorepo for building universal Reason React applications with synchronized store state, realtime delivery, JSON websocket actions, and IndexedDB-backed browser persistence on top of a Dream server.

The repository currently ships with ecommerce, todo, todo-multiplayer, and poker demos that exercise the shared packages.

Note: parts of this README and `docs/` are AI-generated drafts. Human review and editing are required before treating them as finalized documentation.

## Status

This project is still a prototype.

- APIs, package boundaries, and schemas are not stable and are subject to change
- the ecommerce app is a demo, not a production-ready product
- expect refactors while the core abstractions settle

## Repository layout

```text
packages/
  reason-realtime/
    dream-middleware/      Dream websocket middleware with pluggable adapters
    pgnotify-adapter/      PostgreSQL LISTEN/NOTIFY adapter
  universal-reason-react/
    components/            Universal document components for SSR + hydration
    lucide-icons/          Universal Lucide icon rendering for SSR + hydration
    router/                Shared nested routing for Dream + Reason React
    store/                 Runtime store builders and browser persistence

demos/
  ecommerce/
    server/                Dream server demo
    shared/                Shared ecommerce domain types for native + Melange
    ui/                    Reason React / Melange frontend demo
  todo/
    server/                Minimal SSR + hydration demo
    ui/                    Minimal client demo
  todo-multiplayer/
    server/                PostgreSQL-backed realtime todo demo
    shared/                Shared schema + model types
    ui/                    Realtime todo frontend
  poker/
    server/                Deterministic SSR lobby, native Dream server, and Jev API
    shared/                Hold'em engine, evaluator, and computer rules
    ui/                    Responsive six-handed single-player table
```

## What is in the store package?

`packages/universal-reason-react/store` is designed so stores do not have to use realtime sync.

Current runtime pieces:

- `StoreBuilder.Runtime` - public store builder API for local-only and synced stores
- `StoreOffline` - internal offline-first runtime implementation
- `StoreIndexedDB` - browser persistence for confirmed snapshots and action ledgers
- `StoreActionLedger` - queued synced actions with ack and retry metadata

The ecommerce demo uses these runtimes in two different ways:

- inventory store = synced runtime with IndexedDB confirmed snapshots plus action ledger support
- cart store = local-only runtime with IndexedDB confirmed snapshots and cross-tab sync

## Monorepo subpackages

The packages in this monorepo are meant to be used together to build a universal `server-reason-react` application in an opinionated way.
- `packages/ocaml-icu4c` is a highly experimental package that uses OCaml FFI to bind to icu4c. It is used to provide parity to the JavaScript Intl API.
- `packages/universal-reason-react/intl` gives you a universal API for calling JavaScript Intl API from native OCaml and JS using ocaml-icu4c under the hood.
- `packages/universal-reason-react/router` gives you one route tree for Dream and the browser, including SSR entrypoints.
- `packages/universal-reason-react/store` gives you the Tilia-backed store authoring model, hydration, persistence, and realtime sync.
- `packages/universal-reason-react/lucide-icons` keeps SVG icon rendering consistent across server and client.
- `packages/reason-realtime/dream-middleware` exposes the Dream websocket endpoint and middleware plumbing.
- `packages/reason-realtime/pgnotify-adapter` turns PostgreSQL `LISTEN/NOTIFY` into realtime events that can feed the store.

In practice, the opinionated stack looks like this:

- Dream handles HTTP routing, SSR, and websocket endpoints
- `universal-reason-react/router` shares route definitions between Dream and Reason React
- `universal-reason-react/components` renders the document shell consistently on server and client
- `universal-reason-react/store` hydrates initial state and keeps it reactive in the browser
- `reason-realtime/*` pushes server-side changes into the client store after the initial SSR render and accepts mutation commands over the same websocket connection

If you want to understand how the pieces fit together, start with the ecommerce demo and the package-level docs under `docs/`.

## Realtime architecture

Realtime support is split into packages under `packages/reason-realtime`.

- `dream-middleware` exposes the Dream websocket middleware and adapter interface
- `pgnotify-adapter` is the first adapter and uses PostgreSQL `LISTEN/NOTIFY`

The ecommerce demo uses this stack to stream inventory updates into the client store.

The todo-multiplayer demo uses the same stack for both reads and writes:

- initial state is rendered into the HTML payload
- the browser subscribes to `/_events`
- UI actions send JSON mutation envelopes over that websocket
- PostgreSQL triggers broadcast patches back to the subscribed list channel

Current Dune public libraries publish under the `resync.*` namespace, including:

- `resync.reason_realtime_*`
- `resync.common_*`
- `resync.universal_reason_react_*`

## Universal rendering

Universal document helpers live in `packages/universal-reason-react/components`.

Universal icon rendering lives in `packages/universal-reason-react/lucide-icons`.

These packages are used by the ecommerce demo for:

- server-rendered HTML shell generation
- client hydration
- matching Lucide SVG output on server and client
- injected scripts and serialized store state

## Tech stack

- OCaml / Reason syntax
- Dream
- server-reason-react
- Melange
- React
- Tilia
- PostgreSQL
- pnpm
- esbuild

## Running the demos

Install JavaScript dependencies from the repo root:

```bash
pnpm install
```

### Linux Docker workflow

To verify the demos build in a Linux container:

```bash
docker compose run --rm linux-check
```

To run the demos in Linux containers:

```bash
docker compose up ecommerce-demo
docker compose up todo-demo
docker compose up poker-demo
```

The root `docker-compose.yml` provisions a Debian-based image with opam, pnpm,
and the native build dependencies, then builds the monorepo using container
volumes for the opam root, `_build`, and `node_modules`.

The `postgres` service also initializes the ecommerce schema from
`demos/ecommerce/server/sql`. Because Postgres only runs init scripts for a new
data directory, rerun with a fresh volume when you need to re-seed:

```bash
docker compose down -v
docker compose up ecommerce-demo
```

### Ecommerce demo

Start PostgreSQL:

```bash
docker compose up -d postgres
```

Configure environment variables. For development, `.envrc` is recommended:

```bash
# .envrc
export DB_URL="postgres://executor:executor-password@localhost:5432/executor_db"
export API_BASE_URL="http://localhost:8899"
export ECOMMERCE_DOC_ROOT="./_build/default/demos/ecommerce/ui/src"
export TODO_DOC_ROOT="./_build/default/demos/todo/ui/src"
export TODO_MP_DOC_ROOT="./_build/default/demos/todo-multiplayer/ui/src"
export POKER_DOC_ROOT="./_build/default/demos/poker/ui/src"
export POKER_SERVER_PORT="8090"
```

The server fails fast unless `DB_URL` and `ECOMMERCE_DOC_ROOT` are set.

Run the demo (requires two terminals):

```bash
# Terminal 1: Build watch
pnpm ecommerce:dune:watch

# Terminal 2: Server with restart on UI changes
pnpm ecommerce:watch
```

> **Note:** Dune's built-in watch (`dune exec -w`) is not recommended for live development. Use `pnpm ecommerce:watch` instead, which restarts the server when UI artifacts change.

Open:

```text
http://localhost:8899
```

### Todo demo

Run the demo (requires two terminals):

```bash
# Terminal 1: Build watch
pnpm todo:dune:watch

# Terminal 2: Run server (restarts on UI changes)
pnpm todo:watch
```

> **Note:** Set `TODO_DOC_ROOT` in `.envrc` as shown above. The server fails fast unless this is set.

> **Note:** Dune's built-in watch (`dune exec -w`) is not recommended for live development. Use `pnpm todo:watch` instead, which restarts the server when UI artifacts change.

Open:

```text
http://localhost:8080
```

### Poker demo

River Room is a six-handed single-player Texas Hold'em game with five computer opponents. Opponents can use TypeSafe Jev decision models or deterministic local rules. The SSR lobby is deterministic; shuffling starts only after an explicit deal action in the browser. No database is needed, and all chips are play chips only.

Run the demo (requires two terminals):

```bash
# Terminal 1: Build watch
pnpm poker:dune:watch

# Terminal 2: Run server (restarts on UI changes)
pnpm poker:watch
```

For a one-time build and server run:

```bash
pnpm poker:build
pnpm poker:serve
```

> **Note:** Set `POKER_DOC_ROOT` in `.envrc` as shown above. `POKER_SERVER_PORT` defaults to `8090`. See `demos/poker/README.md` and `demos/poker/.envrc.example` for the complete setup and Docker instructions.

Jev is the default opponent mode when configured. Set `TYPESAFE_API_KEY` in the native server's untracked environment and optionally set `JEV_MODEL` (default `jev-latest`). The browser only learns availability, the configured model name, and metadata from actual model answers; it never receives the API key. Each upstream request contains only the acting opponent's two hole cards plus public cards, betting/stack state, position, personality, public action history, and legal options. It never contains the deck, future cards, human hole cards, or another opponent's hole cards. With no key or on a request failure, the table clearly falls back once to local rules without retrying the paid request. The **Local** selector always works.

Open http://localhost:8090. Run the focused native engine suite with `pnpm poker:test:unit`.

### Todo multiplayer demo

The todo-multiplayer demo shares the root PostgreSQL service and applies both schema DDL and generated realtime triggers.

Run the demo (requires two terminals):

```bash
# One-time or after schema changes
pnpm todo-mp:db:init

# Terminal 1: Build watch
pnpm todo-mp:dune:watch

# Terminal 2: Run server (restarts on UI changes)
pnpm todo-mp:watch
```

> **Note:** Set `TODO_MP_DOC_ROOT` in `.envrc` as shown above. The server fails fast unless this is set.

> **Note:** `pnpm todo-mp:db:init` regenerates `demos/todo-multiplayer/server/sql/generated/realtime.sql` before applying the schema and triggers.

### Ecommerce database setup

The ecommerce demo also has a one-shot database bootstrap command outside Docker.

```bash
# One-time or after schema changes
pnpm ecommerce:db:init
```

> **Note:** `pnpm ecommerce:db:init` regenerates `demos/ecommerce/server/sql/generated/realtime.sql` before applying the initial schema and triggers.

Open:

```text
http://localhost:8898
```

### Available scripts

| Script | Description |
|--------|-------------|
| `ecommerce:db:init` | Apply ecommerce schema and generated realtime triggers |
| `ecommerce:dune:watch` | Dune build watch for ecommerce |
| `ecommerce:watch` | Run ecommerce server, restart on .build_stamp changes |
| `todo:dune:watch` | Dune build watch for todo |
| `todo:watch` | Run todo server, restart on .build_stamp changes |
| `poker:build` | Build poker browser assets and native server |
| `poker:dune:watch` | Dune build watch for poker |
| `poker:serve` | Run the poker server after a build |
| `poker:watch` | Run poker server, restart on .build_stamp changes |
| `poker:test:unit` | Run native Alcotest poker engine tests |
| `todo-mp:schema:generate` | Generate todo-multiplayer realtime SQL artifacts |
| `todo-mp:db:init` | Apply todo-multiplayer schema and generated realtime triggers |
| `todo-mp:dune:watch` | Dune build watch for todo-multiplayer |
| `todo-mp:watch` | Run todo-multiplayer server, restart on .build_stamp changes |

## Build

Build specific demos from the repo root:

```bash
dune build @ecommerce-app @ecommerce-server
dune build @todo-app @todo-server
dune build @poker-app @poker-server
dune build demos/todo-multiplayer/ui/src/.build_stamp demos/todo-multiplayer/server/src/server.exe
```

Or build all:

```bash
dune build @all-apps @all-servers
```

## Default development configuration

The ecommerce demo currently uses:

- `ECOMMERCE_DOC_ROOT=./_build/default/demos/ecommerce/ui/src`
- `DB_URL=postgres://executor:executor-password@localhost:5432/executor_db`
- `API_BASE_URL=http://localhost:8899`
- server port `8899`

The todo demo uses:

- `TODO_DOC_ROOT=./_build/default/demos/todo/ui/src`
- server port `8080`

The todo-multiplayer demo uses:

- `TODO_MP_DOC_ROOT=./_build/default/demos/todo-multiplayer/ui/src`
- `DB_URL=postgres://executor:executor-password@localhost:5432/executor_db`
- server port `8898`

The poker demo uses:

- `POKER_DOC_ROOT=./_build/default/demos/poker/ui/src`
- server interface `127.0.0.1` (Docker overrides it to `0.0.0.0`)
- server port `8090`

## Demo behavior

- the poker server renders only the deterministic lobby; explicit browser actions deal and play complete local hands with side pots, split-pot settlement, busted seats, and table restart
- inventory is rendered on the server and hydrated in the browser
- inventory realtime updates come from PostgreSQL notifications over websockets
- sold-out equipment shows a disabled “Out of stock” button on the listing and item pages; restocking re-enables Add to cart through realtime updates
- the cart is client-side and persisted to IndexedDB
- reservation type selection updates the displayed equipment rate between supported units
- ecommerce Live Support and llm-chat share `ChatMessageView` in `packages/reason-streamdown` for assistant Markdown and plain-text user messages
- the demo still uses `reason-react-day-picker` in the storefront UI

## Notes for contributors

- the current build flow is Dune + Melange + esbuild
- the universal packages intentionally keep separate `js/` and `native/` layouts to preserve the existing build pipeline
- the repo is mid-refactor, so README details should follow the code and scripts in the repo rather than older planning notes

## Vision

The long-term goal is to make the Dream + universal Reason React + store stack reusable across multiple apps, with demos living beside the shared packages instead of inside one application tree.
