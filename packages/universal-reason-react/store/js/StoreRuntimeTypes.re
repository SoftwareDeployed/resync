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

type streamsConfig('patch, 'stream_event, 'streaming_state) = {
  decodeStreamEvent: StoreJson.json => option('stream_event),
  emptyStreamingState: 'streaming_state,
  reduceStream: ('streaming_state, 'stream_event) => 'streaming_state,
  reconcilePatch: ('patch, 'streaming_state) => 'streaming_state,
};
