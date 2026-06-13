# Hydration Error: SSR Query Cache Not Populated

## Current State

The `pnpm todo-mp:test:browser` test fails with a hydration mismatch:
- Server renders: `Loading...`
- Client expects: actual data (or `Loading...` if consistent)

## Root Cause

The query cache is empty in SSR output:
```html
<script type="text/json" id="query-cache">{"queries":[":"],"results":{}}</script>
```

The `results: {}` is empty - no queries were executed.

## What Should Happen

1. SSR renders `HomePage` component
2. `UseQuery` hooks register queries with `QueryRegistry`
3. All registered queries are executed against the database
4. Results are serialized into the HTML as `query-cache` script tag
5. Client hydrates the query cache before rendering
6. Client sees same data as server → no hydration mismatch

## What Actually Happens

1. `EntryServer.re` does a pre-render pass to collect queries
2. It calls `QueryRegistry.with_registry` with a database connection
3. It calls `renderToString(prerenderElement)` to trigger query registration
4. Then it calls `execute_queries()` and `get_results()`

**Problem**: The pre-render element (`prerenderElement`) is built outside the `QueryRegistry.with_registry` callback:

```reason
// Line 88-107 - prerenderElement is built OUTSIDE with_registry
let prerenderServerState: Routes.serverState = {store, serializedQueries: ""};
let prerenderApp = <UniversalRouter ... />;
let prerenderDocument = UniversalRouter.renderDocument(...);
let prerenderElement = <TodoStore.Context.Provider value=store> prerenderDocument </TodoStore.Context.Provider>;

// Line 114-130 - with_registry callback
QueryRegistry.with_registry(~db=(module Db), ~f=() => {
  let _html = ReactDOM.renderToString(prerenderElement);  // prerenderElement was built OUTSIDE the registry context!
  let* () = QueryRegistry.execute_queries();
  let snapshot = QueryRegistry.get_results();
  ...
});
```

The `prerenderElement` was built **before** `with_registry` was called, so when `renderToString` runs inside the callback, the `QueryRegistry` context is active but the element references queries that don't exist in the registry.

## Also: State Element ID Mismatch

From the HTML output:
```html
<script type="text/json" id="initial-store">...</script>
```

But `Hydration.re` looks for:
```reason
let parseState = (~stateElementId: string, ...) =>
  switch (stateElementId->getElementById->Js.Nullable.toOption) { ... }
```

The `stateElementId` defaults to `"initial-store"` but we need to verify this matches what's rendered.

## Fixes Needed

### Fix 1: Move prerender inside with_registry

The prerender element must be built **inside** the `QueryRegistry.with_registry` callback so that when `UseQuery` hooks run, they're already inside the registry context.

```reason
QueryRegistry.with_registry(~db=(module Db), ~f=() => {
  // Build prerender element INSIDE the registry context
  let prerenderServerState: Routes.serverState = {store, serializedQueries: ""};
  let prerenderApp = <UniversalRouter ... />;
  let prerenderDocument = UniversalRouter.renderDocument(...);
  let prerenderElement = <TodoStore.Context.Provider value=store> prerenderDocument </TodoStore.Context.Provider>;
  
  // Now render - queries will be registered
  let _html = ReactDOM.renderToString(prerenderElement);
  let* () = QueryRegistry.execute_queries();
  let snapshot = QueryRegistry.get_results();
  let serializedQueries = QueryRegistry.serialize_snapshot(snapshot);
  Lwt.return(serializedQueries);
});
```

### Fix 2: Verify state element ID

Ensure `Hydration.re` uses the same element ID that `EntryServer.re` renders.

## Files to Modify

1. `demos/todo-multiplayer/server/src/EntryServer.re` - Move prerender logic inside `with_registry` callback
2. Possibly `packages/universal-reason-react/store/js/Hydration.re` - Verify element ID

## Test Command

```bash
pnpm todo-mp:test:browser
```

## References

- `packages/universal-reason-react/store/js/QueryRegistry.re` - SSR query collection
- `packages/universal-reason-react/store/js/UseQuery.re` - useQuery hook
- `demos/todo-multiplayer/server/src/EntryServer.re` - SSR entry point
