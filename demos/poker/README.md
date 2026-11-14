# River Room poker demo

A six-handed, single-player Texas Hold'em table built with universal Reason React. The native Dream server renders a deterministic lobby; the first hand is shuffled and dealt only after the player presses **Deal first hand** in the browser. Later hands deal automatically after a 2.8-second result pause when the human and at least one opponent still have chips.

The table supports preflop, flop, turn, and river betting; fold, check, call, bet, raise, and all-in actions; short and full all-ins; side pots; seven-card hand evaluation; split pots and odd chips; dealer and blind rotation; busted seats; repeated hands; and a full table restart. All five opponents use deterministic local rules in `shared/js/PokerComputer.re`. There is no network decision service or Jev integration in this demo branch.

## Presentation and sound

Every face-up card uses an inline, path-based SVG suit with its full card name retained for assistive technology. Hole cards, card backs, and community cards have staggered deal-in motion; the active avatar has a subtle ring and changed actions have a brief highlight. These animations use opacity and transforms without moving seat anchors, and `prefers-reduced-motion` selects static content.

Sound is **off by default** and is enabled only with the **Sound off / Sound on** toggle. Audio objects are created lazily in the browser for deals, folds, chip actions, board cards, and wins; turning sound off pauses loaded clips. The five OGG files and their license are served locally from `ui/src/static/sounds`, with Dune copying them into the static document root.

The clips come from **Kenney Casino Audio 1.1** by Kenney Vleugels, released under **Creative Commons Zero (CC0)**. See `ui/src/static/sounds/License.txt`, https://kenney.nl/assets/casino-audio, and https://opengameart.org/content/54-casino-sound-effects-cards-dice-chips. No audio is hosted externally.

When a hand leaves the human at zero chips, an accessible **You are felted** dialog appears. Its autofocused **Restart table** button resets the game and dismisses the dialog. Restarting also cancels pending hand timers.

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
