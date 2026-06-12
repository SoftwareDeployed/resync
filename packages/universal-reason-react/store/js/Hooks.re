/** Low-level hooks API for standalone query and mutation wiring.
 *  
 *  Runtime stores expose `StoreDef.Hooks` with store-scoped `useQuery`
 *  and auto-dispatched `useMutation`. This module is for manual wiring:
 *  - `useQuery` - Subscribe to query results with SSR support; pass `~skip=true` to keep hook order stable without subscribing
 *  - `useQueryOption` - Subscribe only when params are `Some(params)`, treating `None` like a skipped query
 *  - `useMutation` - Build a typed mutation handle with an explicit dispatch callback
 *  - `useMutationFn` - Build a typed async mutation function with an explicit dispatch callback
 *  - `useIsQueryLoading` - Check if a query is in loading state
 */
let useQuery = UseQuery.useQuery;
let useQueryOption = UseQuery.useQueryOption;
let useMutation = UseMutation.make;
let useMutationFn = UseMutation.makeFn;
let useIsQueryLoading = UseQuery.useIsQueryLoading;
