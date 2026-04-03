# Todo Demo App

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.


A minimal working example of a Universal Reason React application.

This is the canonical first-app walkthrough for the docs and the recommended starting point for new examples.

This demo shows:
- Server-side rendering (SSR)
- Client-side hydration
- Shared routing between server and client
- Type-safe configuration sharing

## Project Structure

```
demos/todo/
├── ui/             # Client-side UI (Melange/Reason React)
└── server/         # Dream server (OCaml/Reason)
```

## Running the Demo

### Prerequisites

Install dependencies from the repo root:

```bash
pnpm install
```

Configure environment variables. For development, add to `.envrc` at the repo root:

```bash
export TODO_DOC_ROOT="./_build/default/demos/todo/ui/src/"
```

### Run the Demo

Run with pnpm (requires two terminals):

```bash
# Terminal 1: Build watch
pnpm todo:dune:watch

# Terminal 2: Run server (restarts on UI changes)
pnpm todo:watch
```

> **Note:** Set `TODO_DOC_ROOT` in `.envrc` as shown above. The server fails fast unless this is set.

> **Note:** Dune's built-in watch (`dune exec -w`) is not recommended for live development. Use `pnpm todo:watch` instead, which restarts the server when UI artifacts change.

Open http://localhost:8080 in your browser.

## How It Works

### 1. Store Types (`ui/src/TodoStore.re`)

Defines the `todo` type, `config` type, and store implementation used by both server and client.

### 2. UI Components (`ui/src/`)

- `HomePage.re` - The main page component
- `Routes.re` - Route definitions using UniversalRouter
- `Index.re` - Client entry point that hydrates the server-rendered HTML

### 3. Server (`server/src/`)

- `EntryServer.re` - Defines `getServerState` and `render` functions
- `server.ml` - Dream server setup and route handlers

### 4. The SSR Flow

1. **Server receives request** → Matches route → Calls `getServerState`
2. **Server renders HTML** → Includes serialized state in a `<script>` tag
3. **Browser loads page** → Displays server-rendered HTML immediately
4. **Client JavaScript loads** → `Index.re` hydrates the app
5. **App is interactive** → Client takes over without re-fetching data

## Key Files to Study

- `ui/src/TodoStore.re` - How types and store are defined
- `ui/src/Routes.re` - How routes are defined
- `server/src/EntryServer.re` - How SSR state is provided
- `ui/src/Index.re` - How hydration works

## Next Steps

To make this a fully functional todo app, you would add:

1. **State management** - Use `universal-reason-react/store` for reactive state
2. **Database** - Add PostgreSQL with Caqti for persistence
3. **Real-time updates** - Use `reason-realtime` for live sync
4. **Styling** - Add CSS or a UI framework

See the [ecommerce demo](../ecommerce/) for a complete implementation with all these features.
