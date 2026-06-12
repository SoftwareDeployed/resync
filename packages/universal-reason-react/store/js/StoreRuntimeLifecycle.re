/* StoreRuntimeLifecycle - Promise-based lifecycle tracking for stores.

   Tracks bootstrap readiness, persistence queue depth, and pending optimistic
   action count so consumers can await specific store states without coupling
   to React or DOM APIs. */

type t = {
  storeName: string,
  readyPromiseRef: ref(Js.Promise.t(unit)),
  readyResolveRef: ref(option(unit => unit)),
  readyRejectRef: ref(option(unit => unit)),
  persistenceQueueRef: ref(Js.Promise.t(unit)),
  pendingPersistenceRef: ref(int),
  pendingActionIdsRef: ref(array(string)),
  connectionRef: ref(StoreRuntimeTypes.connection_status),
  statusListenersRef: StoreEvents.callback_registry(StoreRuntimeTypes.status),
};

[@platform js]
external setTimeout: (unit => unit, int) => unit = "setTimeout";

[@platform native]
let setTimeout = (_callback, _timeout) => ();

let make = (~storeName, ()) => {
  let resolveRef: ref(option(unit => unit)) = ref(None);
  let rejectRef: ref(option(unit => unit)) = ref(None);
  let readyPromise =
    Js.Promise.make((~resolve, ~reject) => {
      resolveRef := Some(v => resolve(. v));
      rejectRef := Some(_ => reject(. Failure("StoreRuntimeLifecycle boot rejected")));
    });
  {
    storeName,
    readyPromiseRef: ref(readyPromise),
    readyResolveRef: resolveRef,
    readyRejectRef: rejectRef,
    persistenceQueueRef: ref(Js.Promise.resolve()),
    pendingPersistenceRef: ref(0),
    pendingActionIdsRef: ref([||]),
    connectionRef: ref(StoreRuntimeTypes.NotApplicable),
    statusListenersRef: ref([||]),
  };
};

let currentStatus = (lifecycle: t): StoreRuntimeTypes.status => {
  let pendingActions = Array.length(lifecycle.pendingActionIdsRef^);
  {
    ready: lifecycle.readyResolveRef^ == None,
    idle: lifecycle.readyResolveRef^ == None && lifecycle.pendingPersistenceRef^ == 0 && pendingActions == 0,
    connection: lifecycle.connectionRef^,
    pendingPersistence: lifecycle.pendingPersistenceRef^,
    pendingActions,
  };
};

let notifySubscribers = (lifecycle: t) => {
  StoreEvents.Callback.emit(
    ~registry=lifecycle.statusListenersRef,
    currentStatus(lifecycle),
  );
};

let trackBoot = (lifecycle: t, bootPromise: Js.Promise.t('a)): Js.Promise.t('a) => {
  let onSettled = () => {
    switch (lifecycle.readyResolveRef^) {
    | Some(resolve) =>
      lifecycle.readyResolveRef := None;
      resolve(());
    | None => ()
    };
    notifySubscribers(lifecycle);
  };
  let guarded =
    Js.Promise.then_(
      v => {
        onSettled();
        Js.Promise.resolve(v);
      },
      bootPromise,
    )
    |> Js.Promise.catch(_err => {
         onSettled();
         Js.Promise.reject(Failure("StoreRuntimeLifecycle trackBoot failed"));
       });
  lifecycle.readyPromiseRef := guarded |> Js.Promise.then_(_ => Js.Promise.resolve());
  guarded;
};

let trackPersistenceOp = (lifecycle: t, op: unit => Js.Promise.t('a)): Js.Promise.t('a) => {
  lifecycle.pendingPersistenceRef := lifecycle.pendingPersistenceRef^ + 1;
  notifySubscribers(lifecycle);
  let onDone = () => {
    lifecycle.pendingPersistenceRef :=
      lifecycle.pendingPersistenceRef^ > 0 ? lifecycle.pendingPersistenceRef^ - 1 : 0;
    notifySubscribers(lifecycle);
  };
  let chained =
    lifecycle.persistenceQueueRef^
    |> Js.Promise.then_(() =>
         try({
           op()
           |> Js.Promise.then_(result => {
                onDone();
                Js.Promise.resolve(result);
              })
           |> Js.Promise.catch(_err => {
                onDone();
                Js.Promise.reject(Failure("StoreRuntimeLifecycle trackPersistenceOp failed"));
              })
         }) {
         | _err =>
           onDone();
           Js.Promise.reject(Failure("StoreRuntimeLifecycle trackPersistenceOp failed"))
         }
       );
  lifecycle.persistenceQueueRef :=
    chained
    |> Js.Promise.then_(_ => Js.Promise.resolve())
    |> Js.Promise.catch(_ => Js.Promise.resolve());
  chained;
};

let trackPersistence = (lifecycle: t, op: Js.Promise.t('a)): Js.Promise.t('a) =>
  trackPersistenceOp(lifecycle, () => op);


let markActionPending = (lifecycle: t, actionId: string) => {
  if (!(lifecycle.pendingActionIdsRef^)->Js.Array.some(~f=id => id == actionId)) {
    lifecycle.pendingActionIdsRef :=
      (lifecycle.pendingActionIdsRef^)->Js.Array.concat(~other=[|actionId|]);
    notifySubscribers(lifecycle);
  };
};

let markActionSettled = (lifecycle: t, actionId: string) => {
  let pending =
    (lifecycle.pendingActionIdsRef^)->Js.Array.filter(~f=id => id != actionId);
  if (Array.length(pending) != Array.length(lifecycle.pendingActionIdsRef^)) {
    lifecycle.pendingActionIdsRef := pending;
    notifySubscribers(lifecycle);
  };
};

let markConnectionWaiting = (lifecycle: t) => {
  lifecycle.connectionRef := StoreRuntimeTypes.WaitingForOpen;
  notifySubscribers(lifecycle);
};

let markConnectionOpen = (lifecycle: t) => {
  lifecycle.connectionRef := StoreRuntimeTypes.Open;
  notifySubscribers(lifecycle);
};

let markConnectionNotApplicable = (lifecycle: t) => {
  lifecycle.connectionRef := StoreRuntimeTypes.NotApplicable;
  notifySubscribers(lifecycle);
};

let whenReady = (~timeout: int=10000, lifecycle: t): Js.Promise.t(unit) =>
  Js.Promise.make((~resolve, ~reject) => {
    let settled = ref(false);
    let _ = lifecycle.readyPromiseRef^ |> Js.Promise.then_(() => {
      if (!settled.contents) {
        settled := true;
        let unitValue = ();
        resolve(. unitValue);
      };
      Js.Promise.resolve();
    }) |> Js.Promise.catch(_ => {
      if (!settled.contents) {
        settled := true;
        reject(. Failure("StoreRuntimeLifecycle.whenReady failed"));
      };
      Js.Promise.resolve();
    });
    setTimeout(() => {
      if (!settled.contents) {
        settled := true;
        reject(. Failure("StoreRuntimeLifecycle.whenReady timed out after " ++ string_of_int(timeout) ++ "ms"));
      };
    }, timeout);
    ();
  });

let whenIdle = (~timeout: int=10000, lifecycle: t): Js.Promise.t(unit) =>
  Js.Promise.make((~resolve, ~reject) => {
    let settled = ref(false);
    let listenerIdRef: ref(option(StoreEvents.listener_id)) = ref(None);
    let cleanup = () =>
      switch (listenerIdRef.contents) {
      | Some(listenerId) =>
        StoreEvents.Callback.unlisten(
          ~registry=lifecycle.statusListenersRef,
          listenerId,
        );
        listenerIdRef := None;
      | None => ()
      };
    let resolveIfIdle = (status: StoreRuntimeTypes.status) => {
      if (!settled.contents && status.idle) {
        settled := true;
        cleanup();
        let unitValue = ();
        resolve(. unitValue);
      };
    };
    let _ =
      whenReady(~timeout, lifecycle)
      |> Js.Promise.then_(() => {
           let listenerId =
             StoreEvents.Callback.listen(
               ~registry=lifecycle.statusListenersRef,
               status => resolveIfIdle(status),
             );
           listenerIdRef := Some(listenerId);
           resolveIfIdle(currentStatus(lifecycle));
           Js.Promise.resolve();
         })
      |> Js.Promise.catch(_err => {
           if (!settled.contents) {
             settled := true;
             cleanup();
             reject(. Failure("StoreRuntimeLifecycle whenIdle failed"));
           };
           Js.Promise.resolve();
         });
    setTimeout(() => {
      if (!settled.contents) {
        settled := true;
        cleanup();
        reject(. Failure("StoreRuntimeLifecycle.whenIdle timed out after " ++ string_of_int(timeout) ++ "ms"));
      };
    }, timeout);
    ();
  });

let status = (lifecycle: t): StoreRuntimeTypes.status => currentStatus(lifecycle);

let subscribeStatus =
    (lifecycle: t, callback: StoreRuntimeTypes.status => unit): StoreEvents.listener_id =>
  StoreEvents.Callback.listen(~registry=lifecycle.statusListenersRef, callback);

let unsubscribeStatus = (lifecycle: t, listenerId: StoreEvents.listener_id) =>
  StoreEvents.Callback.unlisten(~registry=lifecycle.statusListenersRef, listenerId);
