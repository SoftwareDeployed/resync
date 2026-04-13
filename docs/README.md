# Universal Reason React Documentation

A comprehensive framework for building universal (SSR + client-side) Reason React applications with Dream server, Tilia state management, and real-time capabilities.

## Overview

This documentation covers the complete stack for building universal Reason React applications:

- **Universal Router**: Shared routing between server and client
- **Store System**: Tilia-backed runtime stores with synchronous SSR hydration and IndexedDB persistence
- **Real-time Sync**: WebSocket-based live updates and typed JSON actions via PostgreSQL LISTEN/NOTIFY
- **Component System**: Universal rendering helpers and icon support

## Quick Start

**New to Resync?** Start with the **[Getting Started Guide](GETTING-STARTED.md)** for a step-by-step tutorial to build a PostgreSQL-backed realtime app with synced store, queries, and mutations.

### Prerequisites

- OCaml toolchain (opam)
- PostgreSQL database
- Node.js (for bundling)

### Installation

```bash
# Clone the repository
git clone https://github.com/SoftwareDeployed/resync
cd resync

# Install dependencies
opam install . --deps-only

# Set up environment
cp .envrc.example .envrc
# Edit .envrc with your database credentials

# Build the project
dune build
```

### Your First App

Use the checked-in `demos/todo` demo as the first app walkthrough.

`demos/todo` is a minimal universal Reason React app that demonstrates:

- SSR and route handling in `server/`
- client hydration in `ui/`

```bash
export TODO_DOC_ROOT="./_build/default/demos/todo/ui/src/"
pnpm run todo:dune:watch
pnpm run todo:watch
```

Open `http://localhost:8080`.

Key files to inspect:

- `demos/todo/server/src/EntryServer.re` — SSR state and render integration
- `demos/todo/ui/src/Index.re` — hydration entry point
- `demos/todo/ui/src/Routes.re` — route configuration

After the basics are working, move to:

- [todo-multiplayer](../demos/todo-multiplayer/) for SSR + websocket mutations + PostgreSQL-backed realtime patches
- [ecommerce demo](../demos/ecommerce/) for a larger multi-store realtime app

## Package Documentation

### Getting Started

- **[GETTING-STARTED.md](GETTING-STARTED.md)** - Step-by-step guide to build a PostgreSQL-backed realtime app with synced store, queries, and mutations. The recommended starting point for new users.

### Core Packages

- **[universal-reason-react/router](universal-reason-react.router.md)** - Shared nested routing for Dream and Reason React. Define routes once, use them everywhere.
- **[universal-reason-react/store](universal-reason-react.store.md)** - Tilia-backed store authoring with SSR hydration, persistence, and real-time sync.
- **[universal-reason-react/components](universal-reason-react.components.md)** - Universal rendering helpers and shared components.
- **[universal-reason-react/lucide-icons](universal-reason-react.lucide-icons.md)** - Universal Lucide icon rendering for server and client.
- **[universal-reason-react/intl](universal-reason-react.intl.md)** - Universal internationalization library with `Intl.NumberFormatter` and `Intl.DateTimeFormatter` for both server (via ICU4C) and client (via native `Intl`).
- **[ocaml-icu4c](ocaml-icu4c.md)** - Low-level OCaml bindings for ICU4C (International Components for Unicode), used internally by the intl package for native targets.

### Real-time Packages

- **[reason-realtime/stream](reason-realtime.stream.md)** - Streaming primitives for SSE, NDJSON, and Dream stream helpers.
- **[reason-realtime/dream-middleware](reason-realtime.dream-middleware.md)** - Dream websocket middleware for server-to-client realtime delivery and JSON action handling.
- **[reason-realtime/pgnotify-adapter](reason-realtime.pgnotify-adapter.md)** - PostgreSQL LISTEN/NOTIFY adapter for database-driven real-time events.

### SQL-First Realtime Guides

- **[SQL annotations](realtime-schema.sql-annotations.md)** - Annotated SQL syntax for tables, queries, mutations, and broadcasts.
- **[SQL queries](realtime-schema.queries.md)** - Named realtime queries and how to use them from server code.
- **[SQL mutations](realtime-schema.mutations.md)** - Named realtime mutations and mutation handling flow.
- **[Generated artifacts](realtime-schema.generated-artifacts.md)** - What the generated SQL, snapshot, and migrations contain.
- **[Streaming lifecycle](realtime.streaming-lifecycle.md)** - End-to-end flow from mutation dispatch to realtime patch delivery.

### Setup Guides

- **[Dream Router Store Setup](dream-router-store-setup.md)** - Complete guide for wiring a Dream app with router and store.
- **[Testing and Coverage](testing.md)** - Browser and native test inventory, commands, and current coverage guidance.

## Architecture Overview

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   Dream Server  │◄────│  Universal Router │────►│   Client App    │
│   (OCaml/Reason)│     │   (Shared Routes) │     │   (Reason React)│
└────────┬────────┘     └──────────────────┘     └─────────────────┘
         │                                           │
         │                                           │
         ▼                                           ▼
┌─────────────────┐                         ┌─────────────────┐
│  PostgreSQL DB  │◄────────────────────────►│ Runtime Store   │
│  LISTEN/NOTIFY  │   WebSocket Events      │ + RealtimeClient│
└─────────────────┘                         └─────────────────┘
```

### Key Concepts

1. **Universal Rendering**: Routes and components render identically on server and client
2. **SSR Hydration**: Server renders initial state, client hydrates without refetching
3. **Source State Pattern**: Single source of truth with derived projections
4. **Real-time Patches**: Typed patch system for live updates without full re-renders
5. **WebSocket Actions**: UI actions can send typed JSON actions over the active realtime socket

## Development

### Running the Demos

```bash
# todo-multiplayer
pnpm run todo-mp:db:init
pnpm run todo-mp:dune:watch
pnpm run todo-mp:watch

# ecommerce
pnpm run ecommerce:db:init
pnpm run ecommerce:dune:watch
pnpm run ecommerce:watch
```

### Building Documentation

```bash
# Generate API docs from .rei files
dune build @doc
```

## API Status

⚠️ **Prototype Warning**: These APIs are not stable and are subject to change. Expect breaking changes as we refine the authoring experience.

## Contributing

When updating documentation:
1. Keep code examples runnable and tested
2. Update the ecommerce demo if core patterns change
3. Mark experimental APIs with `[@mel.experimental]`
4. Remove AI-generated disclaimers after human review

## Additional Resources

- [Ecommerce Demo](../demos/ecommerce/) - Reference implementation
- [Package Source](../packages/) - Source code for all packages
- [Dream Framework](https://github.com/aantron/dream) - Web framework
- [Tilia](https://github.com/darklang/tilia) - Reactive state management
