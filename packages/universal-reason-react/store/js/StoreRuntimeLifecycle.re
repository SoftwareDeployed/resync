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
};

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
  };
};

let trackBoot = (lifecycle: t, bootPromise: Js.Promise.t('a)): Js.Promise.t('a) => {
  let onSettled = () => {
    switch (lifecycle.readyResolveRef^) {
    | Some(resolve) =>
      lifecycle.readyResolveRef := None;
      resolve(());
    | None => ()
    };
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
         Js.Promise.resolve(Obj.magic());
       });
  lifecycle.readyPromiseRef := guarded |> Js.Promise.then_(_ => Js.Promise.resolve());
  guarded;
};

let trackPersistence = (lifecycle: t, op: Js.Promise.t('a)): Js.Promise.t('a) => {
  lifecycle.pendingPersistenceRef := lifecycle.pendingPersistenceRef^ + 1;
  let onDone = () =>
    lifecycle.pendingPersistenceRef :=
      lifecycle.pendingPersistenceRef^ > 0 ? lifecycle.pendingPersistenceRef^ - 1 : 0;
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
         Js.Promise.resolve(Obj.magic());
       });
  let chained =
    lifecycle.persistenceQueueRef^ |> Js.Promise.then_(() => wrapped);
  lifecycle.persistenceQueueRef := chained |> Js.Promise.then_(_ => Js.Promise.resolve());
  wrapped;
};

let trackAsync = (_lifecycle: t, op: Js.Promise.t('a)): Js.Promise.t('a) => op;

let markActionPending = (lifecycle: t, _actionId: string) => {
  lifecycle.pendingActionsRef := lifecycle.pendingActionsRef^ + 1;
};

let markActionSettled = (lifecycle: t, _actionId: string) => {
  lifecycle.pendingActionsRef :=
    lifecycle.pendingActionsRef^ > 0 ? lifecycle.pendingActionsRef^ - 1 : 0;
};

let markConnectionWaiting = (lifecycle: t) => {
  lifecycle.connectionRef := StoreRuntimeTypes.WaitingForOpen;
};

let markConnectionOpen = (lifecycle: t) => {
  lifecycle.connectionRef := StoreRuntimeTypes.Open;
};

let markConnectionNotApplicable = (lifecycle: t) => {
  lifecycle.connectionRef := StoreRuntimeTypes.NotApplicable;
};

let whenReady = (lifecycle: t): Js.Promise.t(unit) => lifecycle.readyPromiseRef^;

let rec whenIdle = (lifecycle: t): Js.Promise.t(unit) =>
  lifecycle.readyPromiseRef^
  |> Js.Promise.then_(() =>
       lifecycle.persistenceQueueRef^
       |> Js.Promise.then_(() =>
            if (lifecycle.pendingActionsRef^ > 0) {
              Js.Promise.resolve() |> Js.Promise.then_(() => whenIdle(lifecycle));
            } else {
              Js.Promise.resolve();
            }
          )
     );

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
