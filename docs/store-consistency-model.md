# Store Consistency Model

This document describes the consistency guarantees, lifecycle, error handling, and browser-testing patterns of the Resync store runtime. It is anchored to the currently implemented code in `packages/universal-reason-react/store/js/`.

## Consistency Model Overview

The Resync store runtime implements **last-writer-wins-by-timestamp**.

- The newest timestamp always wins.
- **Server-confirmed state is the convergence point** for synced stores.
- There is **no field-level merge**: entire state objects are swapped.
- There are **no vector clocks**.
- There is **no CRDT** semantics.

For local stores, the confirmed state is simply the result of the most recent `dispatch`. For synced stores, optimistic state may temporarily diverge from the server-confirmed state, but it reconverges deterministically as acks, patches, and snapshots arrive.

## Local Store Lifecycle

The local-only runtime lives in `StoreOffline.re:145-280` (`Local.Make`). Its lifecycle is:

1. **Synchronous SSR hydration** — `hydrateStore` (lines 176-244) parses the server-rendered payload from the DOM element identified by `stateElementId`. If parsing fails, it falls back to `emptyState`. On the server, `hydrateStore` returns a store built from `emptyState` directly.

2. **Mount and source capture** — On the client, `StoreSource.make` mounts immediately after hydration. The mount callback captures the source actions, opens a `BroadcastChannel` named `resync.store.<storeName>`, and begins the background reconcile.

3. **`reconcilePersistedState`** (lines 145-174) — Reads the `confirmed_state` record from IndexedDB for the current `scopeKey`. If the persisted record has a strictly greater timestamp than the current source state, it calls `setExternalState` to overwrite the source. If the current state wins (or there is no persisted record), the current state is written back to IndexedDB via `writeStateRecord`.

4. **`writeStateRecord` / `persistState`** (lines 73-143) — `writeStateRecord` persists `{scopeKey, state, timestamp}` to the IndexedDB `confirmed_state` object store. `persistState` wraps `writeStateRecord` and, unless `~broadcast=false`, also broadcasts the state across tabs.

5. **Cross-tab confirmed-state propagation** — The `BroadcastChannel` handler (lines 199-230) receives messages containing `{scopeKey, timestamp, state}`. When the incoming `scopeKey` matches and the incoming `timestamp` is strictly greater than the current state’s timestamp, the runtime calls `setExternalState` to adopt the newer confirmed state.

## Synced Store Lifecycle

The synced runtime lives in `StoreOffline.re:580-1298` (`Synced.Make`). Its lifecycle is:

1. **Synchronous SSR hydration** — `hydrateStore` (lines 1154-1264) behaves like the local runtime: it parses the SSR state synchronously, sets `confirmedStateRef` to that initial state, and on the server returns a store built from it immediately.

2. **Boot reconcile** — On the client mount, the runtime reads the persisted `confirmed_state` from IndexedDB, then calls `StoreRuntimeHelpers.selectHydrationBase` (`StoreRuntimeHelpers.re:19-31`) to choose between the SSR state and the persisted state. `selectHydrationBase` uses a **strict `>` comparison** on timestamps; equal timestamps keep the SSR/initial state.

3. **`refreshOptimisticState`** (lines 698-770) — After the hydration base is chosen, the runtime replays optimistic actions on top of the confirmed state. It reads the IndexedDB action ledger for the current scope, filters out stale `Acked` records (those whose UUID timestamp is `<=` the confirmed timestamp), deletes them, and then re-applies all `Pending` and `Syncing` records by calling `Schema.reduce` in enqueue order. If IndexedDB fails during replay, the catch branch falls back to the confirmed state without the optimistic overlay.

4. **`startSubscription`** (lines 1081-1152) — Opens a `RealtimeClient.Socket.subscribeSynced` websocket connection for the store’s subscription. It surfaces lifecycle events (`Open`, `Reconnect`, `Close`, `ConnectionError`) through the store’s typed event emitter and resumes any pending actions once the socket opens.

5. **`handleAck`** (lines 955-1050) — Processes server acknowledgements. Status `"ok"` marks the action settled, updates the ledger to `Acked`, and emits `ActionAcked`. Status `"error"` marks the action settled, updates the ledger to `Failed`, calls `refreshOptimisticState` to roll back the optimistic state to confirmed plus remaining pending actions, and emits `ActionFailed`.

6. **`handlePatch`** (lines 1063-1079) and **`handleSnapshot`** (lines 1052-1061) — Apply server-delivered changes to `confirmedStateRef`, persist the new confirmed state to IndexedDB (`persistConfirmedState`), broadcast it across tabs, and then call `refreshOptimisticState` to rebuild the optimistic overlay.

7. **Optimistic replay from the IndexedDB action ledger** — The optimistic state is always derived from `confirmedStateRef` plus the remaining `Pending`/`Syncing` actions in the ledger. This means cross-tab updates do not depend solely on websocket delivery; another tab writing an optimistic action to the shared ledger will trigger `refreshOptimisticState` via the `BroadcastChannel` `"optimistic_action"` message.

## Lifecycle API

Every runtime module generated by `StoreBuilder.buildLocal`, `StoreBuilder.buildSynced`, or `StoreBuilder.buildCrud` conforms to the `StoreBuilder.Runtime.Exports` module type. The lifecycle surface is:

- **`whenReady`** — Returns a `Js.Promise.t(unit)` that resolves when the store has completed boot (IndexedDB read finished and, for synced stores, the subscription is ready). Implemented in `StoreRuntimeLifecycle.re:138-162` with a default timeout of **10000ms**. The promise races against the boot completion and the timeout; if the timeout wins, it rejects.

- **`whenIdle`** — Returns a `Js.Promise.t(unit)` that resolves when the store is ready, the persistence queue is empty, and there are no pending optimistic actions. Implemented in `StoreRuntimeLifecycle.re:164-199` with the same default 10000ms timeout. It recursively waits if new actions arrive while draining.

- **`status`** — Returns a snapshot of the current `StoreRuntimeTypes.status` (defined in `StoreRuntimeTypes.re:11-17`). The record contains:
  - `ready: bool`
  - `idle: bool`
  - `connection: StoreRuntimeTypes.connection_status` (`NotApplicable | WaitingForOpen | Open`)
  - `pendingPersistence: int`
  - `pendingActions: int`

- **`subscribeStatus`** / **`unsubscribeStatus`** — Subscribe to or unsubscribe from status changes. `subscribeStatus` returns a `status_listener_id` string. Listeners are notified on every lifecycle change (ready, idle, connection, pending counts). Implemented in `StoreRuntimeLifecycle.re:213-225`.

- **`flushCache`** — Returns the `whenIdle` promise. This is a convenience alias exposed on the runtime module.

## Error Handling

### IndexedDB replay failure fallback

In `StoreOffline.re:747-756` (`refreshOptimisticState` catch branch), if reading or replaying the IndexedDB action ledger fails, the runtime logs a warning, falls back to `confirmedStateRef`, and sets the source state to the confirmed state without the optimistic overlay. This removes the pending UI changes but leaves the store in a known-good state.

### Readiness and idle timeouts

`StoreRuntimeLifecycle.re` gates both `whenReady` (lines 138-162) and `whenIdle` (lines 164-199) with a `setTimeout` race. The default timeout is **10000ms** and can be overridden with the `~timeout` labeled argument. If the timeout fires first, the promise rejects with `Failure("StoreRuntimeLifecycle.whenReady timed out after ...")` or the idle equivalent.

### Websocket error and disconnect surfacing

`startSubscription` (`StoreOffline.re:1127-1134`) forwards websocket `onError` callbacks into the public event surface as `StoreEvents.ConnectionError(message)` and also invokes the optional schema `onError` hook. Close events are emitted as `StoreEvents.Close`. Reconnect events are emitted as `StoreEvents.Reconnect`.

### Ack-error rollback path

When the server returns `status: "error"` in `handleAck` (`StoreOffline.re:1005-1047`), the runtime:
1. Marks the action settled in the lifecycle tracker.
2. Persists the action ledger record with `Failed` status and the error message.
3. Calls `refreshOptimisticState` to rebuild state from confirmed plus the remaining pending actions, effectively rolling back the failed action.
4. Emits `ActionFailed` only after the ledger has been updated.

### Promise rejection hygiene

Catch blocks in the runtime no longer use `Obj.magic`. Rejections are explicit: `Js.Promise.reject(Failure("..."))`. This is visible in `StoreRuntimeLifecycle.re` (e.g., lines 77-79, 101-103, 148-153) and in `StoreOffline.re` (e.g., line 749-756).

## Browser Testing Patterns

The canonical browser-test helpers live in `packages/playwright/js/BrowserTestUtils.re`.

### `waitForIDBContent`

`BrowserTestUtils.waitForIDBContent` (lines 49-86) polls IndexedDB directly via `Playwright.evaluateString` (raw `page.evaluate()`). It opens the database by `dbName`, reads all records from the `confirmed_state` object store, stringifies each `value`, and resolves when any record contains the expected substring. It polls every 50ms with a 10-second timeout. This is the sound way to verify persistence without modifying application code.

### `seedConfirmedStateBeforeNavigation`

`BrowserTestUtils.seedConfirmedStateBeforeNavigation` (lines 91-123) uses `Playwright.addInitScript` to inject a script before any page JavaScript runs. The injected script opens IndexedDB, creates the `confirmed_state` and `actions` object stores if needed, and writes a record with shape `{scopeKey, value, timestamp}`. This lets tests set up persisted state before the store hydrates.

### `waitForBodyNotContains`

`BrowserTestUtils.waitForBodyNotContains` (lines 127-144) polls `document.body.innerText` every 100ms (10-second timeout) and resolves when the unexpected text disappears. It uses `Playwright.evaluateString` to run the polling loop in the browser context.

### Direct page evaluation primitives

- `Playwright.evaluateString` — evaluate an arbitrary JavaScript string in the page context and return a promise.
- `Playwright.addInitScript` — register a script that runs before page scripts on every navigation.

### Canonical store names and record shapes

- Canonical local store names: `todo.simple`, `ecommerce.cart`.
- Canonical synced store name: `todo-multiplayer`.
- `confirmed_state` record shape: `{scopeKey, value, timestamp}`.
- Default `scopeKey` for global local stores: `"default"`.

### Cross-tab testing

To share IndexedDB and `BroadcastChannel` between tabs, create pages from the **same browser context**, not from separate `browser.newPage` calls:

```reason
let context = browser->Playwright.newContext;
let pageA = context->Playwright.newPageInContext;
let pageB = context->Playwright.newPageInContext;
```

### Rollback testing

To test the ack-error rollback path, the todo-multiplayer UI exposes a **"Fail Query"** button that triggers the `FailServerMutation` action. Browser tests click this button and then assert that the optimistic UI change reverts after the server returns the error ack. Do not rely on window globals or magic strings; interact with the actual UI button.

## Limitations and Future Work

- **IDB-failure recovery is documented but not browser-automated in this phase.** The `refreshOptimisticState` catch branch handles runtime recovery, but there is no dedicated Playwright test that forces an IndexedDB failure and asserts the fallback behavior.

- **Equal timestamps prefer the SSR / initial state.** Because `StoreRuntimeHelpers.selectHydrationBase` uses a strict `>` comparison, a persisted state with exactly the same timestamp as the SSR state will not replace it. Applications must ensure `timestampOfState` is monotonic if they expect IndexedDB to win.

- **The model is eventual consistency, not serializable or strongly consistent.** Optimistic state can diverge temporarily. Convergence depends on ordered delivery of server patches and acks. There are no transactions or linearizability guarantees across multiple stores.

- **No field-level merge.** Conflicts are resolved by swapping the entire state object. If two writers update disjoint fields, the later timestamp still overwrites the whole object.

- **No conflict resolution beyond timestamp comparison.** There is no three-way merge, no operational transform, and no application-defined merge function. Timestamp comparison is the single resolution mechanism.
