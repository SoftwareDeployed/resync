/* StoreRuntimeTypes - Shared types for store runtime lifecycle.

   Lives in a separate module to avoid dependency cycles between
   StoreBuilder, StoreRuntimeLifecycle, and StoreOffline. */

type connection_status =
  | NotApplicable
  | WaitingForOpen
  | Open;

type status = {
  ready: bool,
  idle: bool,
  connection: connection_status,
  pendingPersistence: int,
  pendingActions: int,
};
