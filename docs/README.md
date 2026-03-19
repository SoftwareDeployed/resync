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
git clone <repository-url>
cd executor-full-stack

# Install dependencies
opam install . --deps-only

# Set up environment
cp .envrc.example .envrc
# Edit .envrc with your database credentials

# Build the project
dune build
```

### Your First App

Let's build a simple todo list app from scratch. This example demonstrates the complete flow: server-side rendering, client hydration, and state management.

#### Project Structure

```
todo-app/
├── dune-project
├── dune-workspace
├── server/
│   ├── dune
│   └── src/
│       └── server.ml
├── ui/
│   ├── dune
│   └── src/
│       ├── Index.re
│       ├── Routes.re
│       └── HomePage.re
└── shared/
    ├── dune
    └── src/
        └── Config.re
```

#### 1. Project Configuration

**dune-project:**
```lisp
(lang dune 3.0)
(name todo-app)

(generate_opam_files true)

(package
 (name todo-app)
 (synopsis "A simple todo app")
 (depends
  (ocaml (>= 4.14.0))
  dune
  dream
  reason
  (universal-reason-react-router (>= 0.1.0))
  (universal-reason-react-store (>= 0.1.0))
  (server-reason-react (>= 0.1.0))
  lwt))
```

**dune-workspace:**
```lisp
(lang dune 3.0)

(context default)
(context
 (default
  (name browser)
  (profile release)))
```

#### 2. Shared Types

**shared/src/Config.re:**
```reason
[@deriving json]
type todo = {
  id: string,
  text: string,
  completed: bool,
};

type config = {
  todos: list(todo),
};
```

**shared/dune:**
```lisp
(library
 (name todo_shared)
 (libraries reason)
 (preprocess (pps ppx_deriving_json)))
```

#### 3. UI Components

**ui/src/HomePage.re:**
```reason
[@react.component]
let make = () => {
  <div className="container">
    <h1>{React.string("My Todo List")}</h1>
    <p>{React.string("Coming soon...")}</p>
  </div>;
};
```

**ui/src/Routes.re:**
```reason
open UniversalRouter;

let router =
  create(
    ~document=document(~title="Todo App", ()),
    ~notFound=(module NotFoundPage),
    [
      index(~id="home", ~page=(module HomePage), ()),
    ],
  );

// 404 page
module NotFoundPage = {
  [@react.component]
  let make = () => {
    <div>{React.string("Page not found")}</div>;
  };
};
```

**ui/src/Index.re:**
```reason
let root = ReactDOM.querySelector("#root") |. Belt.Option.getExn;

ReactDOM.hydrateRoot(
  root,
  <UniversalRouter router=Routes.router />,
);
```

**ui/dune:**
```lisp
(library
 (name todo_ui)
 (modes melange)
 (libraries
  todo_shared
  universal_reason_react_router_js
  reason-react
  melange.js))
```

#### 4. Server

**server/src/server.ml:**
```reason
let getServerState = (context: UniversalRouterDream.serverContext) => {
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  
  (* Return some sample todos for the initial state *)
  let config: Config.config = {
    todos: [
      {id: "1"; text: "Learn ReasonML"; completed: false};
      {id: "2"; text: "Build an app"; completed: false};
    ];
  };
  
  Lwt.return(UniversalRouterDream.State(config));
};

let render = (~context, ~serverState: Config.config, ()) => {
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let serverPath = UniversalRouterDream.contextPath(context);
  let serverSearch = UniversalRouterDream.contextSearch(context);

  let app =
    <UniversalRouter
      router=Routes.router
      routeRoot
      serverPath
      serverSearch
    />;

  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~routeRoot,
      ~path=serverPath,
      ~search=serverSearch,
      ~serializedState=Config.config_to_json(serverState) |> Js.Json.stringify,
      (),
    );

  document;
};

let app =
  UniversalRouterDream.app(
    ~router=Routes.router,
    ~getServerState,
    ~render,
    (),
  );

let () =
  Dream.run ~port:8080
  @@ Dream.logger
  @@ Dream.router([
    Dream.get "/**" (UniversalRouterDream.handler ~app),
  ]);
```

**server/dune:**
```lisp
(executable
 (name server)
 (libraries
  dream
  server-reason-react.react
  server-reason-react.reactDom
  universal_reason_react_router_native
  todo_shared
  todo_ui))
```

#### 5. Build and Run

```bash
# Create the project structure
mkdir -p todo-app/{server,ui,shared}/src
cd todo-app

# Create all the files above...

# Install dependencies
opam install . --deps-only

# Build the project
dune build

# Run the server
dune exec -- ./server/src/server.exe

# Open http://localhost:8080 in your browser
```

#### What This Demonstrates

1. **Server-Side Rendering**: The server renders the initial HTML with sample data
2. **Universal Router**: Same routes work on server and client
3. **SSR Hydration**: Client takes over without re-fetching data
4. **Type Safety**: Shared types between server and client

#### Next Steps

- Add a store for state management ([see store docs](universal-reason-react.store.md))
- Add client-side interactivity (add todos, mark complete)
- Add a database for persistence
- See the [ecommerce demo](../demos/ecommerce/) for a complete example

See the [ecommerce demo](../demos/ecommerce/) for a complete working example.

## Package Documentation

### Core Packages

- **[universal-reason-react/router](universal-reason-react.router.md)** - Shared nested routing for Dream and Reason React. Define routes once, use them everywhere.
- **[universal-reason-react/store](universal-reason-react.store.md)** - Tilia-backed store authoring with SSR hydration, persistence, and real-time sync.
- **[universal-reason-react/components](universal-reason-react.components.md)** - Universal rendering helpers and shared components.
- **[universal-reason-react/lucide-icons](universal-reason-react.lucide-icons.md)** - Universal Lucide icon rendering for server and client.

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

⚠️ **Prototype Warning**: These APIs are actively evolving. While the core concepts are stable, expect breaking changes as we refine the authoring experience.

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
