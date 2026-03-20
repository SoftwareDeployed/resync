# Universal Reason React Documentation

A comprehensive framework for building universal (SSR + client-side) Reason React applications with Dream server, Tilia state management, and real-time capabilities.

## Overview

This documentation covers the complete stack for building universal Reason React applications:

- **Universal Router**: Shared routing between server and client
- **Store System**: Tilia-backed state management with SSR hydration and persistence
- **Real-time Sync**: WebSocket-based live updates via PostgreSQL LISTEN/NOTIFY
- **Component System**: Universal rendering helpers and icon support

## Quick Start

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
dune build demos/todo
export DOC_ROOT="./demos/todo/_build/default/demos/todo/ui/src/app/"
dune exec demos/todo/server/src/server.exe
```

Open `http://localhost:8080`.

Key files to inspect:

- `demos/todo/server/src/EntryServer.re` — SSR state and render integration
- `demos/todo/ui/src/Index.re` — hydration entry point
- `demos/todo/ui/src/Routes.re` — route configuration

After the basics are working, move to the [ecommerce demo](../demos/ecommerce/) for persistence and real-time extensions.

## Package Documentation

### Core Packages

- **[universal-reason-react/router](universal-reason-react.router.md)** - Shared nested routing for Dream and Reason React. Define routes once, use them everywhere.
- **[universal-reason-react/store](universal-reason-react.store.md)** - Tilia-backed store authoring with SSR hydration, persistence, and real-time sync.
- **[universal-reason-react/components](universal-reason-react.components.md)** - Universal rendering helpers and shared components.
- **[universal-reason-react/lucide-icons](universal-reason-react.lucide-icons.md)** - Universal Lucide icon rendering for server and client.
- **[icu-numberformatter](icu-numberformatter.md)** - Native ICU-based number formatting used for server-side currency/decimal/percent output.

### Real-time Packages

- **[reason-realtime/dream-middleware](reason-realtime.dream-middleware.md)** - Dream websocket middleware for server-to-client real-time delivery.
- **[reason-realtime/pgnotify-adapter](reason-realtime.pgnotify-adapter.md)** - PostgreSQL LISTEN/NOTIFY adapter for database-driven real-time events.

### Setup Guides

- **[Dream Router Store Setup](dream-router-store-setup.md)** - Complete guide for wiring a Dream app with router and store.

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
│  PostgreSQL DB  │◄────────────────────────►│  RealtimeClient │
│  LISTEN/NOTIFY  │   WebSocket Events      │  (Store Sync)   │
└─────────────────┘                         └─────────────────┘
```

### Key Concepts

1. **Universal Rendering**: Routes and components render identically on server and client
2. **SSR Hydration**: Server renders initial state, client hydrates without refetching
3. **Source State Pattern**: Single source of truth with derived projections
4. **Real-time Patches**: Typed patch system for live updates without full re-renders

## Development

### Running the Demo

```bash
cd demos/ecommerce
export DB_URL="postgres://user:pass@localhost:5432/db"
export API_BASE_URL="http://localhost:8899"
export DOC_ROOT="./_build/default/demos/ecommerce/ui/src/app/"

dune exec -- ./server/bin/main.exe
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
