# executor

`executor` is an early alpha monorepo for building universal Reason React applications on top of a Dream server, with optional realtime sync and optional client persistence.

The repository currently ships with an ecommerce demo that exercises the shared packages.

## Status

This project is still a prototype.

- APIs, package boundaries, and schemas are still moving
- the ecommerce app is a demo, not a production-ready product
- expect refactors while the core abstractions settle

## Repository layout

```text
packages/
  reason-realtime/
    dream-middleware/      Dream websocket middleware with pluggable adapters
    pgnotify-adapter/      PostgreSQL LISTEN/NOTIFY adapter
  shared-types/            Shared domain types for native + Melange builds
  universal-reason-react/
    components/            Universal document components for SSR + hydration
    store/                 Composable store layers: core, state, sync, persist

demos/
  ecommerce/
    server/                Dream server demo
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

## Realtime architecture

Realtime support is split into packages under `packages/reason-realtime`.

- `dream-middleware` exposes the Dream websocket middleware and adapter interface
- `pgnotify-adapter` is the first adapter and uses PostgreSQL `LISTEN/NOTIFY`

The ecommerce demo uses this stack to stream inventory updates into the client store.

## Universal rendering

Universal document helpers live in `packages/universal-reason-react/components`.

These packages are used by the ecommerce demo for:

- server-rendered HTML shell generation
- client hydration
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

## Running the ecommerce demo

Install JavaScript dependencies from the repo root:

```bash
pnpm install
```

Start PostgreSQL:

```bash
docker compose -f demos/ecommerce/server/docker-compose.yml up -d
```

Run the app from the repo root:

```bash
pnpm dev
```

Open:

```text
http://localhost:8899
```

You can also run the demo from its own directory:

```bash
cd demos/ecommerce
pnpm dev
```

## Build

Build the demo app and server from the repo root:

```bash
dune build @app @server
```

## Default development configuration

The ecommerce demo currently defaults to:

- `DB_URL=postgres://executor:executor-password@127.0.0.1:5432/executor_db`
- `DOC_ROOT=./_build/default/demos/ecommerce/ui/src/app/`
- server port `8899`

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
