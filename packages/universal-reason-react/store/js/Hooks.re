/** Low-level hooks API for standalone query and mutation wiring.
 *  
 *  Runtime stores expose `StoreDef.Hooks` with store-scoped `useQuery`
 *  and auto-dispatched `useMutation`. This module is for manual wiring:
 *  - `useQuery` - Subscribe to query results with SSR support
 *  - `useMutation` - Build a mutation handle with an explicit dispatch callback
 *  - `useIsQueryLoading` - Check if a query is in loading state
 */
let useQuery = UseQuery.useQuery;
let useMutation = UseMutation.make;
let useIsQueryLoading = UseQuery.useIsQueryLoading;
