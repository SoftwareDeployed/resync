/* StoreControllerHelpers: pure mirrors of SyncController ordering semantics.
   Native tests use these helpers to pin production StoreOffline's ordering
   contracts without depending on browser-only runtime state. */

/* Ordering helper for ack/failure handling: returns the continuation sequence
   that enforces ledger-before-event ordering.
   
   The contract: ledger update promise resolves before event emission.
   This helper encodes that sequence without the actual IO. */
let applyAckOrdering = (
   ~updateLedger: unit => Js.Promise.t(unit),
   ~emitEvent: unit => unit,
   (): unit,
 ) => {
   /* The real ordering: ledger update promise resolves, THEN event fires.
      This is enforced by Promise.then_ chaining in the production code. */
   Js.Promise.then_(
     () => {
       emitEvent();
       Js.Promise.resolve();
     },
     updateLedger(),
   );
 };

/* Controller state for emission gating tests.
   Extracted as pure refs so tests can observe and control the state. */
 type emission_state = {
   mutable is_emitting: bool,
   mutable pending_dispatches: array(unit => unit),
 };

 let makeEmissionState = () => {
   is_emitting: false,
   pending_dispatches: [||],
 };

 let isEmitting = (state: emission_state) => state.is_emitting;

 let queueDispatch = (state: emission_state, dispatch_fn: unit => unit) => {
   state.pending_dispatches =
     state.pending_dispatches->Js.Array.concat(~other=[|dispatch_fn|]);
 };

 let drainPendingDispatches = (state: emission_state) => {
   let to_drain = state.pending_dispatches;
   state.pending_dispatches = [||];
   to_drain->Js.Array.forEach(~f=fn => fn());
 };

 let finishEmit = (state: emission_state) => {
   state.is_emitting = false;
   drainPendingDispatches(state);
 };

 /* Core emit with queued dispatch ordering.
    Contract: listeners fire during emission (is_emitting=true),
    nested dispatches are queued, then drained after emission ends.
    
    This is the actual logic from MakeSyncController.emit extracted for testing. */
 let emitWithQueuedDispatch = (
   state: emission_state,
   listeners: array((string, 'a => unit)),
   event: 'a,
) => {
  let snapshot = listeners;
  state.is_emitting = true;

  try({
    snapshot->Js.Array.forEach(~f=((_, listener)) => listener(event));
    finishEmit(state);
  }) {
  | error =>
    finishEmit(state);
    raise(error);
  };
};

 /* Helper to check if a dispatch would be queued or immediate */
 let shouldQueueDispatch = (state: emission_state) => state.is_emitting;
