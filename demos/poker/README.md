# River Room poker demo

A six-handed, single-player Texas Hold'em table built with universal Reason React. The native Dream server renders a deterministic lobby; cards are shuffled and dealt only after the player presses **Deal first hand** in the browser.

The table supports preflop, flop, turn, and river betting; fold, check, call, bet, raise, and all-in actions; short and full all-ins; side pots; seven-card hand evaluation; split pots and odd chips; dealer and blind rotation; busted seats; repeated hands; and a full table restart. All five opponents use deterministic local rules in `shared/js/PokerComputer.re`. There is no network decision service or Jev integration in this demo branch.

## Run locally

From the repository root, install JavaScript dependencies and configure the document root:

```bash
pnpm install
export POKER_DOC_ROOT="./_build/default/demos/poker/ui/src"
export POKER_SERVER_PORT="8090"
# Export these in each terminal, or add them to the repository-root .envrc.
```

The same variables are listed in `demos/poker/.envrc.example`; merge them into an existing `.envrc` rather than replacing settings for other demos.

Build once and start the server:

```bash
pnpm poker:build
pnpm poker:serve
```

For development, use two terminals instead:

```bash
# Terminal 1: build the Melange client and native server on changes
pnpm poker:dune:watch

# Terminal 2: restart the native server when client assets change
pnpm poker:watch
```

Open http://localhost:8090. The server requires `POKER_DOC_ROOT`; it listens on `127.0.0.1:8090` by default. Set `POKER_SERVER_PORT` to change the port and `SERVER_INTERFACE=0.0.0.0` to listen on all interfaces.

The Linux launcher also provisions the opam/pnpm toolchain and starts both watchers:

```bash
./run-linux-demos.sh poker
```

## Run with Docker

From the repository root:

```bash
docker compose up poker-demo
```

Open http://localhost:8090. The service exports `POKER_DOC_ROOT=/workspace/_build/default/demos/poker/ui/src` and binds the server to `0.0.0.0`; no database is required.

## Test and build targets

```bash
# Focused native Alcotest engine suite
pnpm poker:test:unit

# Equivalent Dune targets
dune build @poker-app @poker-server
dune exec demos/poker/tests/native/poker_engine_test.exe
```

The tests cover deck integrity, seven-card evaluation and ties, deterministic SSR/deal behavior, betting contributions, illegal under-raises, short all-ins, fold wins, automatic all-in runouts, side pots, odd chips, and chip conservation.

## Structure

```text
demos/poker/
  shared/js/          Poker types, evaluator, engine, and local decision rules
  shared/native/      Native copies of the same shared sources for Dream/tests
  ui/src/             Universal room, cards, seats, controls, and responsive CSS
  server/src/         Dream static assets and deterministic SSR entry point
  tests/native/       Dune/Alcotest engine regressions
```

`PokerEngine` owns all state transitions and exposes `legalOptions`; both the human controls and local computer decisions go through the same `applyAction` validation. A later server-side decision adapter can select actions without moving betting or settlement rules into the UI.
