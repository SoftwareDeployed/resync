# Store Architecture Refactor Plan

## Overview
Replace the current layered store architecture with a unified offline-first builder where:
- **SSR** provides synchronous initial state for the first render
- **IndexedDB** stores confirmed snapshots and queued actions
- **Tilia store** renders the current optimistic state and derived values
- **Mutations** are strongly typed actions that are queued, acknowledged over the existing websocket, and reconciled asynchronously
- **Stores may opt into sync**; local-only stores use the same action model without websocket delivery

> ⚠️ **Experimental**: This is a prototype. APIs are not stable and subject to change.

## Core Requirements
- UUIDv7 for action IDs with embedded timestamps
- Strongly typed actions and mutation payloads everywhere; do not use string action identifiers
- Refactor the existing `RealtimeClient` and websocket protocol; do not add a parallel websocket stack
- Use JSON-only websocket frames for mutations, acknowledgements, patches, and snapshots
- Keep `hydrateStore(): unit => t` synchronous so SSR hydration stays server-first with no loading state
- Recompute optimistic state from the last confirmed state plus remaining pending or acked actions
- Use `storeName` as the IndexedDB database identity, with a `scopeKey` for route/subscription-scoped stores
- Use a timestamp-based compile-time contract `timestampOfState: state => float`; stores without a natural timestamp must add store-level metadata so SSR, IndexedDB, and realtime snapshots can be compared
- Synced actions must be idempotent under retry; prefer explicit state-setting mutations over toggle-style mutations
- Update all demos: `todo`, `todo-multiplayer`, `ecommerce`
- Follow `docs/DRAFT-PRINCIPALS.md` (minimal setup, no loading states, server-first)
- This is a hard refactor of the builders and demos, not a deprecation path

## Architecture

### Data Flow
1. **Initial Render**: SSR state -> hydrate the Tilia store synchronously.
2. **Background Reconcile**:
   - Open IndexedDB for `storeName`
   - Load the confirmed snapshot for the current `scopeKey`
   - Load pending or acked actions for the same `scopeKey`
   - Choose the newer confirmed state between SSR and IndexedDB using the store timestamp contract
   - Persist the winning confirmed state back to IndexedDB so SSR-only loads still establish a browser snapshot
   - Replay pending or acked actions over that confirmed state to rebuild the optimistic state
3. **Dispatch**:
   - Create `actionId` as UUIDv7
   - Reduce optimistically and update the Tilia source immediately
   - Persist the action ledger and confirmed snapshot metadata to IndexedDB
   - If sync is configured, send a typed action envelope through the refactored `RealtimeClient`
4. **Acknowledgement and Retry**:
   - Server sends a JSON acknowledgement frame with `actionId`, status `ok | error`, and an optional error message
   - `ok` marks the action as `acked`, but it is not pruned from the ledger until a newer confirmed snapshot, patch, or timestamp is observed
   - `error` marks the action as failed, then rebuilds optimistic state from the last confirmed state plus the remaining non-failed actions
   - Transport failures and missing acknowledgements use a simple retry policy: fixed retry delay, small max retry count, and the same `actionId` on every resend
   - Explicit `error` acknowledgements do not retry
5. **Reconciliation**:
   - Patches and snapshots advance the confirmed state
   - Once confirmed state has caught up, prune acked actions from the ledger
6. **Local-only Stores**:
   - If no sync config exists, dispatch still uses the same typed action pipeline
   - Local actions are reduced locally and committed directly to confirmed state
   - Local-only stores do not persist an action ledger
   - Local-only stores broadcast newer confirmed snapshots across tabs for the same `storeName`
7. **Queries**: Read from the Tilia store, which always represents the current optimistic state.

### IndexedDB Schema
Use one IndexedDB database per `storeName`, with per-scope records inside that database.

- `confirmed_state`: `{scopeKey: string, value: state, timestamp: float}`
- `actions`: `{id: UUIDv7, scopeKey: string, action: JSON, status: pending|syncing|acked|failed, enqueuedAt: float, retryCount: int, error: option(string)}`

Notes:
- `storeName` alone is not enough for stores whose state is scoped by route or subscription.
- `scopeKey` is the logical instance of a store inside one `storeName` database.
- `scopeKey` should default to `"default"` for global stores and be derived for scoped stores such as todo list ids or premise ids.
- For local-only stores, only `confirmed_state` is required.

### Wire Protocol
Use JSON-only websocket frames.

Examples:
- Client mutation frame: `{type: "mutation", actionId, action}`
- Server acknowledgement frame: `{type: "ack", actionId, status: "ok" | "error", error: option(string)}`
- Server patch frame: `{type: "patch", ...}`
- Server snapshot frame: `{type: "snapshot", ...}`

Typed actions still decode into strongly typed variants on both client and server; the JSON shape is only the transport encoding.

## Implementation Phases

### Phase 1: Protocol and Foundations
1. Add or update the experimental warning in the relevant docs while the refactor is in progress.
2. Add `UUID.re` with UUIDv7 generation and timestamp extraction helpers.
3. Refactor the websocket mutation flow in `RealtimeClient.re` and `dream-middleware` to support JSON mutation envelopes plus success or error acknowledgement frames.
4. Add a shared action codec pattern for synced stores so client and server can exchange typed actions instead of raw string mutation names.
5. Define the compile-time timestamp contract for stores via `timestampOfState`.
6. Add idempotency handling on the server for synced actions using `actionId` so retries are safe.

### Phase 2: IndexedDB Storage
1. Add `StoreIndexedDB.re` as the browser storage engine, with native no-op implementations where needed.
2. Add `StoreActionLedger.re` for reading, writing, pruning, and replaying queued actions.
3. Persist confirmed snapshots and action ledgers per `storeName` and `scopeKey`.
4. Add startup reconciliation that loads confirmed state plus queued actions and recomputes the optimistic state after SSR hydration.
5. For local-only stores, skip `StoreActionLedger` and persist confirmed state directly.

### Phase 3: Unified Offline-First Builder
Create `StoreBuilder.Runtime.Make` with a simplified schema centered around one state type and one typed action type.

Schema should include:
- State type
- Action type
- `reduce: (~state: state, ~action: action) => state`
- `emptyState`
- `stateElementId`
- `storeName`
- `scopeKeyOfState` or equivalent scoped identity hook
- `timestampOfState`
- State and action JSON codecs
- `makeStore` for derived values and projections
- Optional sync config with subscription resolution and websocket URLs
- Optional failure hook for UI feedback only; rollback itself is handled by replaying from the last confirmed state

Generated API should include:
- `dispatch(action)`: optimistic reduce plus persistence plus optional queueing
- `hydrateStore()`: synchronous SSR bootstrap
- Background reconcile on mount
- `createStore(state)` and `serializeState(state)` for SSR entrypoints

This builder replaces the current split between `Runtime`, `Persisted`, and `Layered` builders.

### Phase 4: Demo Refactors
1. **todo**: Migrate to offline-first local-only typed actions.
2. **ecommerce cart**: Migrate to offline-first local-only typed actions; do not add sync yet.
3. **todo-multiplayer**: Migrate to full offline-first sync with typed actions, acknowledgements, replay, retry, and server-side action decoding.
4. **todo-multiplayer mutation semantics**: Replace non-idempotent toggle-style mutations with explicit state-setting actions so retry remains safe.
5. **ecommerce inventory**: Migrate to the unified builder and realtime snapshot flow. The store must support the action pipeline even if no inventory mutations are used in the first pass.

### Phase 5: SSR and Realtime Integration
1. Server renders initial state plus whatever timestamp metadata the new builder requires.
2. Client hydrates synchronously from SSR with no promise-gated boot path.
3. IndexedDB reconciliation runs after mount and can replace the confirmed base state if it is newer.
4. Websocket subscription starts after mount using the confirmed timestamp.
5. Ack frames, snapshots, and patches all update the same captured store source.
6. Stores without a natural server timestamp must add store metadata so SSR and realtime updates can be ordered correctly.

### Phase 6: Cleanup
1. Remove the old builder split once all demos have been migrated.
2. Remove or replace `StoreLocal`, `StorePersist`, `StoreSync`, and `StoreRuntime` once the unified builder is complete.
3. Update README and package docs to describe the new architecture and typed action flow.

## Verification

### Build Targets
All must build successfully:
- `dune build demos/todo/ui/src/.build_stamp`
- `dune build demos/todo/server/src/server.exe`
- `dune build demos/todo-multiplayer/ui/src/.build_stamp`
- `dune build demos/todo-multiplayer/server/src/server.exe`
- `dune build demos/ecommerce/ui/src/.build_stamp`
- `dune build demos/ecommerce/server/src/server.exe`

### Runtime Verification
Manual or Playwright verification should cover:
- SSR render and hydration without a loading state
- Reload after an optimistic action before acknowledgement arrives
- Offline action queueing and replay after reconnect
- Retry after reconnect or acknowledgement timeout using the same `actionId`
- Ack error handling, including rebuilding from last confirmed state
- Route or subscription switches using distinct `scopeKey` values
- Duplicate patch or acknowledgement delivery not duplicating state
- Server-side idempotency preventing duplicate writes for retried actions

## Open Questions

1. **Simple Retry Defaults**: What acknowledgement timeout, fixed retry delay, and max retry count should V1 use?

2. **Timestamp Source**: For stores without a natural domain timestamp, should the timestamp live as explicit metadata on the store state, on the serialized payload, or both?

3. **Server Idempotency Storage**: Should synced demos use one shared processed-action table, or should each demo manage idempotency in its own schema?
