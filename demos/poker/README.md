# River Room poker demo

A six-handed, single-player Texas Hold'em table built with universal Reason React. The native Dream server renders a deterministic lobby; the first hand is shuffled and dealt only after the player presses **Deal first hand** in the browser. Later hands deal automatically after a 2.8-second result pause when the human and at least one opponent still have chips.

The table supports preflop, flop, turn, and river betting; fold, check, call, bet, raise, and all-in actions; short and full all-ins; side pots; seven-card hand evaluation; split pots and odd chips; dealer and blind rotation; busted seats; repeated hands; and a full table restart. All five opponents can use TypeSafe Jev decision models through the native server, with deterministic local rules in `shared/js/PokerComputer.re` always available as a selectable mode and a transparent fallback. Jev is selected by default whenever the server reports it configured.

## Presentation, decision log, and sound

Every face-up card uses an inline, path-based SVG suit with its full card name retained for assistive technology. Hole cards, card backs, and community cards have staggered deal-in motion; the active avatar has a subtle ring and changed actions have a brief highlight. These animations use opacity and transforms without moving seat anchors, and `prefers-reduced-motion` selects static content.

The **Jev decision log** keeps the eight latest actual model responses, newest first, across hand boundaries. Each entry records the opponent and seat, hand and street, concrete action and sizing, returned model, confidence, and action probabilities with accessible bars. Local-rule and fallback decisions are never logged as Jev. Switching opponent modes or starting another hand preserves the log; **Restart table** clears it.

Sound is **off by default** and is enabled only with the **Sound off / Sound on** toggle. Audio objects are created lazily in the browser for deals, folds, chip actions, board cards, and wins; turning sound off pauses loaded clips. The five OGG files and their license are served locally from `ui/src/static/sounds` through the native `Dream.static` route.

The clips come from **Kenney Casino Audio 1.1** by Kenney Vleugels, released under **Creative Commons Zero (CC0)**. See `ui/src/static/sounds/License.txt`, https://kenney.nl/assets/casino-audio, and https://opengameart.org/content/54-casino-sound-effects-cards-dice-chips. No audio is hosted externally.

When a hand leaves the human at zero chips, an accessible **You are felted** dialog appears. Its autofocused **Restart table** button resets the game and dismisses the dialog. Restarting also cancels pending hand timers and clears the decision log.

## Run locally

From the repository root, install JavaScript dependencies and configure the document root:

```bash
pnpm install
export POKER_DOC_ROOT="./_build/default/demos/poker/ui/src"
export POKER_SERVER_PORT="8090"
# Optional Jev opponents; keep this key only in your untracked environment.
export TYPESAFE_API_KEY="your-local-typesafe-key"
export JEV_MODEL="jev-latest" # optional; this is the default
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

## Jev opponents, privacy, and fallback

`TYPESAFE_API_KEY` is read only by the native Dream server. `JEV_MODEL` defaults to `jev-latest`. `GET /api/poker/jev/status` returns only `available` and the configured `model`; neither that response nor browser code, error responses, or logs contain the API key. Do not put the key in committed files or expose it through a client-side environment variable.

For each computer turn, the browser sends a bounded, explicitly typed snapshot to `POST /api/poker/jev/decision`. The server rejects unknown fields and builds the TypeSafe request itself. The upstream state contains exactly:

- the acting opponent's two hole cards and its cautious/balanced/assertive personality;
- public board and street, pot and betting limits, public stacks and commitments, dealer/blind position, player status, and public action history;
- the legal actions and numeric bet/raise limits for that turn.

The deck, undealt/future cards, the human's hole cards, every other opponent's hole cards, RNG state, and arbitrary client-supplied TypeSafe state are never accepted or transmitted. One TypeSafe request asks for a legal action Choice and a bet/raise-sizing Choice. The server validates the answer and clamps sizing to integer legal limits; the browser still applies it through `PokerEngine.applyAction`, so the engine remains authoritative.

The **Jev / Local** control is always available for local play, while Jev is disabled when not configured. A missing key, network/upstream error, malformed answer, or rejected illegal action produces one clearly identified local-rule decision. Failed paid calls are not silently retried, and late responses from a changed turn or hand are discarded without acting. The rail reports the real returned model, confidence, and action probabilities only after an actual Jev answer; fallback decisions do not display fabricated Jev metadata. All chips are play chips only: there are no accounts, wagers, or cash value.

Each actual Jev response is held for a cancellable 1.5-second thinking interval before its action is applied, so the active seat and decision log remain readable. Changing turns, restarting, or switching modes cancels the pending action.

## Run with Docker

From the repository root:

```bash
# Optional: omit these two exports to use local opponents.
export TYPESAFE_API_KEY="your-local-typesafe-key"
export JEV_MODEL="jev-latest"
docker compose up poker-demo
```

Open http://localhost:8090. The service exports `POKER_DOC_ROOT=/workspace/_build/default/demos/poker/ui/src`, binds the server to `0.0.0.0`, and passes through `TYPESAFE_API_KEY` and `JEV_MODEL` from your shell without storing their values in Compose. No database is required.

## Test and build targets

```bash
# Focused native Alcotest engine suite
pnpm poker:test:unit

# Equivalent Dune targets
dune build @poker-app @poker-server
dune exec demos/poker/tests/native/poker_engine_test.exe
dune exec demos/poker/tests/native/jev_decision_test.exe
```

The engine tests cover deck integrity, seven-card evaluation and ties, deterministic SSR/deal behavior, betting contributions, illegal under-raises, short all-ins, fold wins, automatic all-in runouts, side pots, odd chips, and chip conservation. The separate Jev suite tests strict payload rejection, privacy-bounded upstream state, legal criteria, sizing clamps, and malformed/illegal answer rejection without calling or mocking Jev.

## Structure

```text
demos/poker/
  shared/js/          Poker types, evaluator, engine, and local decision rules
  shared/native/      Native copies of the same shared sources for Dream/tests
  ui/src/             Universal room, cards, seats, controls, and responsive CSS
  server/src/         Dream static assets, SSR, and privacy-bounded Jev API
  tests/native/       Dune/Alcotest engine and Jev validation regressions
```

`PokerEngine` owns all state transitions and exposes `legalOptions`; human controls, Jev answers, and local computer decisions all go through the same `applyAction` validation. `server/src/jev/JevDecision.ml` owns strict payload validation, upstream question construction, answer validation, and chip sizing without duplicating betting or settlement rules.
