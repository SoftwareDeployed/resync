/** Unified hooks API for store queries and mutations.
 *  
 *  Use `open Hooks` in components to access:
 *  - `useQuery` - Subscribe to query results with SSR support
 *  - `useMutation` - Dispatch mutations through the store
 *  - `useIsQueryLoading` - Check if a query is in loading state
 */
let useQuery = UseQuery.useQuery;
let useMutation = UseMutation.make;
let useIsQueryLoading = UseQuery.useIsQueryLoading;
