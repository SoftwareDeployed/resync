# Installation Guide

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.


Complete setup instructions for Universal Reason React applications.

## Prerequisites

### Required Software

- **OCaml** (= 5.4.0) - This version is required
- **opam** (2.1.0 or later)
- **PostgreSQL** (13 or later)
- **Node.js** (18 or later) - for bundling
- **dune** (3.0 or later)

### Platform Support

- Linux (primary)
- macOS
- WSL2 (Windows)

## Quick Setup

### 1. Install OCaml Toolchain

```bash
# Install opam (if not already installed)
# macOS
brew install opam

# Ubuntu/Debian
apt-get install opam

# Initialize opam (if not already initialized)
opam init --bare

eval $(opam env)

# Create a local switch with OCaml 5.4.0 (recommended for this project)
opam switch create . 5.4.0 --deps-only

eval $(opam env)
```

**Note:** Using a local switch (created in the project directory with `opam switch create .`) keeps all dependencies isolated to this project. The `.` creates a switch named after the current directory.

### 2. Clone and Setup Project

```bash
# Clone the repository
git clone https://github.com/SoftwareDeployed/resync
cd resync

# Install dependencies
opam install . --deps-only --with-test

# Alternative: install dev dependencies too
opam install . --deps-only --with-test --with-doc
```

### 3. Setup PostgreSQL

```bash
# Create database
createdb myapp_db

# Create user
createuser -P myapp_user
# Enter password when prompted

# Grant permissions
psql -c "GRANT ALL PRIVILEGES ON DATABASE myapp_db TO myapp_user;"

# Set environment variable
export DB_URL="postgres://myapp_user:password@localhost:5432/myapp_db"
```

### 4. Build the Project

```bash
# Build everything
dune build

# Build specific package
dune build packages/universal-reason-react/router

# Build with watch mode
dune build -w
```

### 4b. Full-stack watch workflow (shared UI + server)

This repository uses a generated stamp file to keep server restarts in sync with
client bundle rebuilds.

```bash
# Terminal 1: keep dune rebuilding artifacts
pnpm run dune:watch

# Terminal 2: run the server executable and restart when the UI stamp changes
pnpm run dev:watch
```

### 5. Run the Demo

```bash
cd demos/ecommerce

# Set environment variables
export DB_URL="postgres://user:pass@localhost:5432/db"
export API_BASE_URL="http://localhost:8080"
export ECOMMERCE_DOC_ROOT="./_build/default/demos/ecommerce/ui/src/"

# Run the server
dune exec -- ./server/bin/main.exe
```

## Development Environment Setup

### VS Code (Recommended)

1. **Install extensions:**
   - OCaml Platform
   - ReasonML
   - Prettier

2. **Configure settings:**

```json
{
  "ocaml.sandbox": {
    "kind": "opam",
    "switch": "${workspaceFolder}"
  },
  "editor.formatOnSave": true,
  "reason.format.width": 80
}
```

### Emacs

```elisp
;; Add to init.el
(use-package reason-mode
  :ensure t)

(use-package merlin
  :ensure t
  :config
  (add-hook 'reason-mode-hook #'merlin-mode))
```

### Vim/Neovim

```vim
" Using vim-plug
Plug 'rescript-lang/vim-rescript'
Plug 'ocaml/vim-ocaml'

" Enable merlin
let g:opamshare = substitute(system('opam config var share'),'\n$','','''')
execute "set rtp+=" . g:opamshare . "/merlin/vim"
```

## Project Structure

```
myapp/
├── dune-project          # Project configuration
├── dune-workspace        # Workspace settings
├── package.json          # JavaScript dependencies
├── .envrc                # Environment variables (direnv)
├── packages/
│   └── myapp/
│       ├── dune
│       └── src/
├── ui/
│   ├── dune
│   └── src/
│       ├── Index.re      # Client entry
│       ├── Routes.re     # Route definitions
│       ├── Store.re      # State management
│       └── components/
└── server/
    ├── dune
    └── src/
        ├── server.ml     # Dream setup
        └── EntryServer.re # SSR entry
```

## Creating a New Project

### 1. Initialize Project

```bash
mkdir myapp
cd myapp

# Create dune-project
# Edit dune-project
```

### 2. Create dune-project

```lisp
(lang dune 3.0)
(name myapp)

(generate_opam_files true)

(source (github username/myapp))
(license MIT)
(authors "Your Name")
(maintainers "Your Name")

 (package
  (name myapp)
  (synopsis "My Universal Reason React App")
  (description "A full-stack Reason React application")
  (depends
   (ocaml (= 5.4.0))
   dune
   dream
   reason
   (universal-reason-react-router (>= 0.1.0))
   (universal-reason-react-store (>= 0.1.0))
   (server-reason-react (>= 0.1.0))
   lwt
   caqti
   caqti-driver-postgresql))
```

### 3. Setup Directory Structure

```bash
mkdir -p ui/src/components ui/src/pages
mkdir -p server/src
mkdir -p shared/src
```

### 4. Create Build Configuration

```lisp
;; dune-workspace
(lang dune 3.0)

(context default)

(context
 (default
  (name browser)
  (profile release)
  (toolchain js)))
```

```lisp
;; ui/dune
(library
 (name myapp_ui)
 (modes melange)
 (libraries
  universal_reason_react_router_js
  universal_reason_react_store_js
  reason-react
  melange.js
  melange.dom))
```

```lisp
;; server/dune
(executable
 (name main)
 (libraries
  dream
  server-reason-react.react
  server-reason-react.reactDom
  universal_reason_react_router_native
  universal_reason_react_store_native
  myapp_shared))
```

```lisp
;; shared/dune
(library
 (name myapp_shared)
 (libraries
  (pps ppx_deriving_json)))
```

### 5. Setup Environment

```bash
# Create .envrc for direnv
cat > .envrc << 'EOF'
export DB_URL="postgres://user:pass@localhost:5432/myapp"
export API_BASE_URL="http://localhost:8080"
export MYAPP_DOC_ROOT="./_build/default/ui/src/"
export PORT=8080
EOF

# Allow direnv
direnv allow

# Or manually source
source .envrc
```

### 6. Initialize Database

```bash
# Create database schema
psql $DB_URL < schema.sql

# Example schema.sql
-- schema.sql
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE IF NOT EXISTS users (
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  email TEXT UNIQUE NOT NULL,
  name TEXT NOT NULL,
  created_at TIMESTAMP DEFAULT NOW()
);

-- Add triggers for realtime
CREATE OR REPLACE FUNCTION notify_changes()
RETURNS TRIGGER AS $$
BEGIN
  PERFORM pg_notify(
    TG_TABLE_NAME || '_changes',
    json_build_object(
      'action', TG_OP,
      'table', TG_TABLE_NAME,
      'data', row_to_json(NEW)
    )::text
  );
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER users_changes_trigger
AFTER INSERT OR UPDATE OR DELETE ON users
FOR EACH ROW EXECUTE FUNCTION notify_changes();
```

### 7. Create Entry Files

```reason
// ui/src/Routes.re
open UniversalRouter;

let router =
  create(
    ~document=document(~title="MyApp", ()),
    ~notFound=(module NotFoundPage),
    [
      index(~id="home", ~page=(module HomePage), ()),
      route(~id="about", ~path="about", ~page=(module AboutPage), [], ()),
    ],
  );
```

```reason
// ui/src/Index.re
let root = ReactDOM.querySelector("#root") |. Belt.Option.getExn;
let store = Store.hydrateStore();

ReactDOM.hydrateRoot(
  root,
  <Store.Context.Provider value=store>
    <UniversalRouter router=Routes.router />
  </Store.Context.Provider>,
);
```

```reason
// server/src/server.ml
let realtime_middleware =
  Middleware.create(
    ~adapter,
    ~resolve_subscription,
    ~load_snapshot,
    (),
  );

let () =
  Dream.run ~port:8080
  @@ Dream.logger
  @@ Dream.router([
    Middleware.route "/_events" realtime_middleware,
    Dream.get "/static/**" (Dream.static Sys.getenv("MYAPP_DOC_ROOT")),
    Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app),
  ]);
```

## Development Workflow

### Building

```bash
# Build all targets
dune build

# Build only UI (client)
dune build ./ui

# Build only server
dune build ./server

# Watch mode
dune build -w

# Clean build
dune clean && dune build
```

### Running

```bash
# Development server
dune exec -- ./server/bin/main.exe

# With environment
db_url=$DB_URL dune exec -- ./server/bin/main.exe

# Hot reload (in separate terminals)
# Terminal 1: dune build -w
# Terminal 2: dune exec --watch ./server/bin/main.exe

# If using the shared UI/server stamp workaround (recommended for this demo):
# Terminal 1: pnpm run dune:watch
# Terminal 2: pnpm run dev:watch
```

### Testing

```bash
# Browser suites
pnpm run framework:test:browser

# Native runtime suite
opam exec -- dune exec ./packages/universal-reason-react/store/test/store_runtime_test.exe

# Native websocket middleware suite
opam exec -- dune exec ./packages/reason-realtime/dream-middleware/test/dream_middleware_test.exe

# Native PostgreSQL notification suite
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
opam exec -- dune exec ./packages/reason-realtime/pgnotify-adapter/test/pgnotify_adapter_test.exe

# Native todo-multiplayer HTTP suite
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
TODO_MP_DOC_ROOT="./_build/default/demos/todo-multiplayer/ui/src/" \
opam exec -- dune exec ./demos/todo-multiplayer/server/test/todo_multiplayer_server_test.exe
```

For a complete test inventory and current coverage status, see `docs/testing.md`.

### Coverage

Behavioral coverage is available immediately through the test inventory in `docs/testing.md` and the suite commands above. OCaml line coverage is available via `bisect_ppx` on the instrumented library dune files (`store/native`, `pgnotify-adapter/src`). See `docs/testing.md` for exact commands.

### Formatting

```bash
# Format all files
dune build @fmt --auto-promote

# Or manually
refmt --in-place ui/src/*.re
ocamlformat --in-place server/src/*.ml
```

### Linting

```bash
# Type check
dune build @check

# Merlin check
ocamlmerlin single errors -filename ui/src/Index.re < ui/src/Index.re
```

## Production Deployment

### Building for Production

```bash
# Production build
dune build --profile release

# Optimize JavaScript
# (Configure esbuild/webpack in your build process)
```

### Docker Deployment

```dockerfile
# Dockerfile
FROM ocaml/opam:alpine-ocaml-5.4

WORKDIR /app

# Install dependencies
COPY --chown=opam:opam . /app
RUN opam install . --deps-only --with-test

# Build
RUN eval $(opam env) && dune build --profile release

# Runtime
EXPOSE 8080
CMD ["_build/default/server/bin/main.exe"]
```

### Environment Variables

Production environment variables:

```bash
# Required
export DB_URL="postgres://user:pass@db-host:5432/myapp"
export PORT=8080

# Optional
export LOG_LEVEL="info"
export STATIC_DIR="/var/www/static"
export SESSION_SECRET="your-secret-key"
```

## Troubleshooting Setup

### opam init fails

```bash
# Initialize without system compiler
opam init --bare --disable-sandboxing

# Or manually configure
opam init --bare

# Create a local switch with OCaml 5.4.0
opam switch create . 5.4.0
```

### Dependencies fail to install

```bash
# Update opam
opam update
opam upgrade

# Install system dependencies
# Ubuntu/Debian
sudo apt-get install libpq-dev libssl-dev pkg-config

# macOS
brew install postgresql openssl pkg-config

# Retry
opam install . --deps-only
```

### dune build fails

```bash
# Clean and rebuild
dune clean
opam install . --deps-only
dune build

# Check merlin is installed
opam install merlin
```

### Database connection issues

```bash
# Test connection
psql $DB_URL -c "SELECT 1"

# Check PostgreSQL is running
pg_isready -h localhost -p 5432

# Verify permissions
psql -c "\du myapp_user"
```

## Next Steps

1. Read [Dream Router Store Setup](dream-router-store-setup.md) for complete integration
2. Explore the [ecommerce demo](../demos/ecommerce/) for a working example
3. Review package documentation:
   - [Router](universal-reason-react.router.md)
   - [Store](universal-reason-react.store.md)
   - [Components](universal-reason-react.components.md)

## Getting Help

- **Documentation**: See `docs/` directory
- **Examples**: Check `demos/` directory
- **Issues**: Report on GitHub
- **Discussions**: GitHub Discussions
