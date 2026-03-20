# resync

`resync` is an early alpha monorepo for building universal Reason React applications with synchronized store state, realtime delivery, and optional client persistence on top of a Dream server.

The repository currently ships with an ecommerce demo that exercises the shared packages.

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
    store/                 Composable store layers: core, state, sync, persist

demos/
  ecommerce/
    server/                Dream server demo
    shared/                Shared ecommerce domain types for native + Melange
    ui/                    Reason React / Melange frontend demo
```

## What is in the store package?

`packages/universal-reason-react/store` is designed so stores do not have to use realtime sync.

Current layers:

- `StoreCore` - schema-driven store construction, projections, and React context
- `StoreState` - SSR serialization and hydration
- `StoreSync` - realtime websocket sync for schema-backed stores
- `StorePersist` - client persistence adapters, currently `localStorage`

The ecommerce demo uses these layers in two different ways:

- inventory store = core + state + sync
- cart store = core + persist

## Monorepo subpackages

The packages in this monorepo are meant to be used together to build a universal `server-reason-react` application in an opinionated way.

- `packages/universal-reason-react/components` gives you the shared document and rendering primitives for SSR + hydration.
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
- `reason-realtime/*` pushes server-side changes into the client store after the initial SSR render

If you want to understand how the pieces fit together, start with the ecommerce demo and the package-level docs under `docs/`.

## Realtime architecture

Realtime support is split into packages under `packages/reason-realtime`.

- `dream-middleware` exposes the Dream websocket middleware and adapter interface
- `pgnotify-adapter` is the first adapter and uses PostgreSQL `LISTEN/NOTIFY`

The ecommerce demo uses this stack to stream inventory updates into the client store.

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

### Ecommerce demo

Start PostgreSQL:

```bash
docker compose -f demos/ecommerce/server/docker-compose.yml up -d
```

Configure environment variables. For development, `.envrc` is recommended:

```bash
# .envrc
export DB_URL="postgres://executor:executor-password@localhost:5432/executor_db"
export API_BASE_URL="http://localhost:8899"
export ECOMMERCE_DOC_ROOT="./_build/default/demos/ecommerce/ui/src"
export TODO_DOC_ROOT="./_build/default/demos/todo/ui/src"
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

### Available scripts

| Script | Description |
|--------|-------------|
| `ecommerce:dune:watch` | Dune build watch for ecommerce |
| `ecommerce:watch` | Run ecommerce server, restart on .build_stamp changes |
| `todo:dune:watch` | Dune build watch for todo |
| `todo:watch` | Run todo server, restart on .build_stamp changes |

## Build

Build specific demos from the repo root:

```bash
dune build @ecommerce-app @ecommerce-server
dune build @todo-app @todo-server
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

## Demo behavior

- inventory is rendered on the server and hydrated in the browser
- inventory realtime updates come from PostgreSQL notifications over websockets
- the cart is client-side and persisted to `localStorage`
- the demo still uses `reason-react-day-picker` in the storefront UI

## Notes for contributors

- the current build flow is Dune + Melange + esbuild
- the universal packages intentionally keep separate `js/` and `native/` layouts to preserve the existing build pipeline
- the repo is mid-refactor, so README details should follow the code and scripts in the repo rather than older planning notes

## Vision

The long-term goal is to make the Dream + universal Reason React + store stack reusable across multiple apps, with demos living beside the shared packages instead of inside one application tree.
