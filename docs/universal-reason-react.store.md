# universal-reason-react/store

Opinionated Tilia-backed store tooling for universal apps.

- Wraps Tilia in a higher-level store authoring model.
- Handles SSR hydration, client persistence, and realtime sync.
- Encourages source-state + derived projections instead of hand-managed React state.

Current building blocks include:

- `StoreBuilder` for defining runtime and persisted stores
- `StoreSource`, `StoreSignal`, and `StoreComputed` for the core Tilia-backed primitives
- `StoreSync` and `RealtimeClient` for websocket-driven updates
- `StorePatch` for typed patch decoding and state updater composition

Use this package when you want:

- SSR bootstrapped state via `getServerState`
- reactive client updates after hydration
- a shared store pattern across server-rendered Reason React apps

This package is part of the current prototype stack and its API is still subject to change.
This document is AI-generated and should be reviewed and edited by a human before being considered final.
