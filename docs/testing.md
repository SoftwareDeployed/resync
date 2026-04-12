# Testing and Coverage

This repo currently has two test layers:

1. **Reason-authored browser tests** that compile to JavaScript and run through Playwright.
2. **Native OCaml backend/runtime tests** that run as small executable test runners through Dune.

The sections below document every authored test currently in the repo, what each test case checks, and how to run the suites.

## Browser test suites

### Aggregate command

```bash
pnpm run framework:test:browser
```

This command builds the browser test targets, starts the built `todo` server, waits for readiness, runs all browser suites, and shuts the server down.

### Individual browser commands

```bash
pnpm run todo:test:browser
pnpm run router:test:browser
pnpm run store:test:browser
pnpm run components:test:browser
pnpm run lucide-icons:test:browser
pnpm run sonner:test:browser
pnpm run intl:test:browser
```

### Browser test inventory

#### `demos/todo/tests/browser/TodoBrowserTest.re`

Runs against the live `todo` demo on `http://127.0.0.1:8080/`.

- **SSR heading** — verifies the SSR page body contains `My Todo List`.
- **SSR todo 1** — verifies the seeded SSR payload includes `Learn ReasonML`.
- **SSR todo 2** — verifies the seeded SSR payload includes `Build an app`.
- **SSR todo 3** — verifies the seeded SSR payload includes `Deploy to production`.
- **SSR stats** — verifies the initial counter shows `0 of 3 completed`.
- **Added todo** — fills `.todo-input`, clicks `.todo-button`, and verifies `Write browser tests` appears.
- **Stats after add** — verifies the counter updates to `0 of 4 completed` after adding a todo.
- **Stats after toggle** — checks `.todo-checkbox` and verifies the counter updates to `1 of 4 completed`.

This is the end-to-end integration test for SSR render, hydration, add flow, and toggle flow.

#### `packages/universal-reason-react/router/test-browser/RouterBrowserTest.re`

- **Document title** — verifies the page title is exactly `Todo App`.
- **Deep link load** — navigates to `/does-not-exist` and verifies the app still renders `My Todo List`.

This covers browser-visible router/title behavior and deep-link fallback handling.

#### `packages/universal-reason-react/store/test-browser/StoreBrowserTest.re`

- **Hydrated seeded state** — verifies the page contains `Learn ReasonML` on initial load.
- **Initial stats** — verifies the initial store-derived count is `0 of 3 completed`.
- **Added todo persists in UI** — adds `Store package browser test` and verifies it renders.
- **Stats after add** — verifies the count becomes `0 of 4 completed`.
- **Stats after toggle** — toggles the first checkbox and verifies the count becomes `1 of 4 completed`.

This covers hydrated store state plus client-side store updates reflected in the UI.

#### `packages/universal-reason-react/components/test-browser/ComponentsBrowserTest.re`

- **Serialized state script** — verifies `#initial-store` contains `Learn ReasonML`.
- **Serialized state includes app data** — verifies the same serialized payload contains `Build an app`.

This covers the document/component layer that emits initial serialized state into the SSR page.

#### `packages/universal-reason-react/lucide-icons/test-browser/LucideIconsBrowserTest.re`

- **Lucide icon SVG rendered** — waits for `.todo-delete svg` and `.todo-delete svg path`.

This covers actual SVG output from the Lucide icon package in the browser-rendered app.

#### `packages/universal-reason-react/sonner/test-browser/SonnerBrowserTest.re`

- **Sonner toast rendered** — opens the generated local HTML harness and waits for visible text `Browser toast message`.

This covers the browser-only toast package in isolation from the demos.

#### `packages/universal-reason-react/intl/test-browser/IntlBrowserTest.re`

- **Intl formatting rendered** — opens the generated local HTML harness and verifies `#result` contains `$1,234.50`.

This covers browser-side number formatting through `Intl.NumberFormatter`.

## Native/backend test suites

### Runtime store tests

Existing native runtime tests live under `packages/universal-reason-react/store/test`.

Build:

```bash
opam exec -- dune build @store-runtime-tests
```

Run:

```bash
opam exec -- dune exec ./packages/universal-reason-react/store/test/store_runtime_test.exe
```

These cover store events, action-ledger helpers, listener behavior, and runtime behavior helpers.

### Dream websocket middleware tests

Build:

```bash
opam exec -- dune build @dream-middleware-tests
```

Run:

```bash
opam exec -- dune exec ./packages/reason-realtime/dream-middleware/test/dream_middleware_test.exe
```

Test cases in `packages/reason-realtime/dream-middleware/test/middleware_behavior_test.ml`:

- **ping replies with pong** — verifies ping handling preserves channel state and emits the expected pong payload.
- **select subscribes and sends snapshot** — verifies `select` subscribes the requested channel and wraps snapshot output.
- **mutation success sends ack ok** — verifies a successful mutation emits an `ack` payload with `status:"ok"`.
- **invalid mutation sends error ack** — verifies malformed mutation frames emit an error ack and trigger close behavior.
- **media handler error sends error frame** — verifies media-handler failures emit an error frame and trigger close behavior.
- **detach unsubscribes active channel** — verifies teardown unsubscribes the active channel from the adapter.

These are native protocol/lifecycle tests for the websocket middleware without spinning up a full server.

### PostgreSQL LISTEN/NOTIFY adapter tests

These tests require a reachable local Postgres and `DB_URL`.

Build:

```bash
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
opam exec -- dune build @pgnotify-adapter-tests
```

Run:

```bash
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
opam exec -- dune exec ./packages/reason-realtime/pgnotify-adapter/test/pgnotify_adapter_test.exe
```

Test cases in `packages/reason-realtime/pgnotify-adapter/test/pgnotify_adapter_behavior_test.ml`:

- **subscribed handler receives patch notification** — starts the adapter, subscribes a channel, issues a real `NOTIFY`, and verifies the handler receives the patch payload.
- **unsubscribe stops later delivery** — unsubscribes the channel, issues another `NOTIFY`, and verifies no handler invocation occurs afterward.

These are real database connectivity tests and they also protect adapter shutdown behavior.

### Todo-multiplayer server HTTP tests

These tests require the todo-multiplayer schema and `DB_URL`/`TODO_MP_DOC_ROOT`.

Prep:

```bash
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
pnpm run todo-mp:db:init
```

Build:

```bash
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
TODO_MP_DOC_ROOT="./_build/default/demos/todo-multiplayer/ui/src/" \
opam exec -- dune build @todo-mp-server-tests
```

Run:

```bash
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
TODO_MP_DOC_ROOT="./_build/default/demos/todo-multiplayer/ui/src/" \
opam exec -- dune exec ./demos/todo-multiplayer/server/test/todo_multiplayer_server_test.exe
```

Test cases in `demos/todo-multiplayer/server/test/server_http_test.ml`:

- **GET / redirects to a created list** — issues a real Dream request to `/` and verifies the server returns a redirect with a created list path.
- **GET invalid uuid route returns not found** — verifies malformed list routes return `404`.
- **GET /favicon.ico returns no content** — verifies the favicon route returns `204 No Content`.

These are native backend HTTP request tests running through the Dream SQL middleware.

## How to measure coverage

## What you can measure today

Today, the repo supports **suite coverage** and **scenario coverage** directly:

1. **Suite coverage** — run the browser and native test commands above and confirm every suite passes.
2. **Scenario coverage** — use the documented test-case inventory in this file to check which behaviors are covered and which are still missing.
3. **Target coverage** — verify that browser-facing packages (`router`, `store`, `components`, `lucide-icons`, `sonner`, `intl`) and the current backend targets (`dream-middleware`, `pgnotify-adapter`, `todo-multiplayer/server`) all have runnable tests.

In practice, the most complete current smoke command is:

```bash
pnpm run framework:test:browser
```

and the native/backend commands are:

```bash
opam exec -- dune exec ./packages/universal-reason-react/store/test/store_runtime_test.exe
opam exec -- dune exec ./packages/reason-realtime/dream-middleware/test/dream_middleware_test.exe
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" opam exec -- dune exec ./packages/reason-realtime/pgnotify-adapter/test/pgnotify_adapter_test.exe
DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" TODO_MP_DOC_ROOT="./_build/default/demos/todo-multiplayer/ui/src/" opam exec -- dune exec ./demos/todo-multiplayer/server/test/todo_multiplayer_server_test.exe
```

## What is **not** wired yet

Line coverage is **not** wired in this repo today.

- `docs/INSTALLATION.md` previously mentioned `dune runtest --instrument-with bisect_ppx`.
- `bisect_ppx` is **not installed** in the current switch.
- The new executable-style native test targets do **not** currently include bisect instrumentation preprocessors.

So if you want line coverage, there is one more setup step to do.

## Line coverage with bisect_ppx

The native test targets are instrumented with `bisect_ppx`. To generate a coverage report:

### 1. Run instrumented tests

Set `BISECT_FILE` so `bisect_ppx` writes `.coverage` files to a known path:

```bash
# Store runtime tests
BISECT_FILE=./bisect_store_ \
opam exec -- dune exec --instrument-with bisect_ppx ./packages/universal-reason-react/store/test/store_runtime_test.exe

# Dream middleware tests
BISECT_FILE=./bisect_middleware_ \
opam exec -- dune exec --instrument-with bisect_ppx ./packages/reason-realtime/dream-middleware/test/dream_middleware_test.exe

# PostgreSQL adapter tests (requires DB_URL)
BISECT_FILE=./bisect_pgnotify_ DB_URL="postgres://executor:executor-password@localhost:5432/executor_db" \
opam exec -- dune exec --instrument-with bisect_ppx ./packages/reason-realtime/pgnotify-adapter/test/pgnotify_adapter_test.exe
```

### 2. Generate the report

```bash
opam exec -- bisect-ppx-report html --coverage-path .
```

This writes an HTML report to `_coverage/index.html` and emits `bisect_*.coverage` files in the repository root during test runs. The `--instrument-with bisect_ppx` flag only affects the build when explicitly requested, leaving normal dev builds untouched.
