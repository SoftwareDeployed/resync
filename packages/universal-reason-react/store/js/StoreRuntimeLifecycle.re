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
  pendingActionsRef: ref(int),
  connectionRef: ref(StoreRuntimeTypes.connection_status),
  statusListenersRef: ref(array((string, StoreRuntimeTypes.status => unit))),
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
    pendingActionsRef: ref(0),
    connectionRef: ref(StoreRuntimeTypes.NotApplicable),
    statusListenersRef: ref([||]),
  };
};

let notifySubscribers = (lifecycle: t) => {
  let currentStatus: StoreRuntimeTypes.status = {
    ready: lifecycle.readyResolveRef^ == None,
    idle: lifecycle.readyResolveRef^ == None && lifecycle.pendingPersistenceRef^ == 0 && lifecycle.pendingActionsRef^ == 0,
    connection: lifecycle.connectionRef^,
    pendingPersistence: lifecycle.pendingPersistenceRef^,
    pendingActions: lifecycle.pendingActionsRef^,
  };
  (lifecycle.statusListenersRef^)->Js.Array.forEach(~f=((_, callback)) =>
    callback(currentStatus)
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

let trackPersistence = (lifecycle: t, op: Js.Promise.t('a)): Js.Promise.t('a) => {
  lifecycle.pendingPersistenceRef := lifecycle.pendingPersistenceRef^ + 1;
  notifySubscribers(lifecycle);
  let onDone = () => {
    lifecycle.pendingPersistenceRef :=
      lifecycle.pendingPersistenceRef^ > 0 ? lifecycle.pendingPersistenceRef^ - 1 : 0;
    notifySubscribers(lifecycle);
  };
  let wrapped =
    Js.Promise.then_(
      result => {
        onDone();
        Js.Promise.resolve(result);
      },
      op,
    )
    |> Js.Promise.catch(_err => {
         onDone();
         Js.Promise.reject(Failure("StoreRuntimeLifecycle trackPersistence failed"));
       });
  let chained =
    lifecycle.persistenceQueueRef^ |> Js.Promise.then_(() => wrapped);
  lifecycle.persistenceQueueRef := chained |> Js.Promise.then_(_ => Js.Promise.resolve());
  wrapped;
};


let markActionPending = (lifecycle: t, _actionId: string) => {
  lifecycle.pendingActionsRef := lifecycle.pendingActionsRef^ + 1;
  notifySubscribers(lifecycle);
};

let markActionSettled = (lifecycle: t, _actionId: string) => {
  lifecycle.pendingActionsRef :=
    lifecycle.pendingActionsRef^ > 0 ? lifecycle.pendingActionsRef^ - 1 : 0;
  notifySubscribers(lifecycle);
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

let rec whenIdle = (~timeout: int=10000, lifecycle: t): Js.Promise.t(unit) =>
  Js.Promise.make((~resolve, ~reject) => {
    let settled = ref(false);
    let _ = whenReady(~timeout, lifecycle) |> Js.Promise.then_(() =>
      lifecycle.persistenceQueueRef^ |> Js.Promise.then_(() =>
        if (lifecycle.pendingActionsRef^ > 0) {
          Js.Promise.resolve() |> Js.Promise.then_(() => {
            if (!settled.contents) {
              let _ =
                whenIdle(~timeout, lifecycle)
                |> Js.Promise.then_(() => { settled := true; let unitValue = (); resolve(. unitValue); Js.Promise.resolve(); })
                |> Js.Promise.catch(_err => { settled := true; reject(. Failure("StoreRuntimeLifecycle whenIdle failed")); Js.Promise.resolve(); });
              ();
            };
            Js.Promise.resolve();
          });
        } else {
          settled := true;
          let unitValue = ();
          resolve(. unitValue);
          Js.Promise.resolve();
        }
      )
    ) |> Js.Promise.catch(_err => {
      settled := true;
      reject(. Failure("StoreRuntimeLifecycle whenIdle failed"));
      Js.Promise.resolve();
    });
    setTimeout(() => {
      if (!settled.contents) {
        settled := true;
        reject(. Failure("StoreRuntimeLifecycle.whenIdle timed out after " ++ string_of_int(timeout) ++ "ms"));
      };
    }, timeout);
    ();
  });

let status = (lifecycle: t): StoreRuntimeTypes.status => {
  let ready = lifecycle.readyResolveRef^ == None;
  let idle = ready && lifecycle.pendingPersistenceRef^ == 0 && lifecycle.pendingActionsRef^ == 0;
  {
    ready,
    idle,
    connection: lifecycle.connectionRef^,
    pendingPersistence: lifecycle.pendingPersistenceRef^,
    pendingActions: lifecycle.pendingActionsRef^,
  };
};

let subscribeStatus = (lifecycle: t, callback: StoreRuntimeTypes.status => unit): string => {
  let listenerId = UUID.make();
  lifecycle.statusListenersRef :=
    Js.Array.concat(~other=[|(listenerId, callback)|], lifecycle.statusListenersRef^);
  listenerId;
};

let unsubscribeStatus = (lifecycle: t, listenerId: string) => {
  lifecycle.statusListenersRef :=
    (lifecycle.statusListenersRef^)->Js.Array.filter(~f=((currentId, _)) =>
      currentId != listenerId
    );
};
