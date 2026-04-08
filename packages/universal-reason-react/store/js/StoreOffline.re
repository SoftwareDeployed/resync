module Local = {
  module type Schema = {
    type state;
    type action;
    type store;

    let reduce: (~state: state, ~action: action) => state;
    let emptyState: state;
    let storeName: string;
    let stateElementId: string;
    let scopeKeyOfState: state => string;
    let timestampOfState: state => float;
    let state_of_json: StoreJson.json => state;
    let state_to_json: state => StoreJson.json;
    let action_of_json: StoreJson.json => action;
    let action_to_json: action => StoreJson.json;
    let makeStore:
      (~state: state, ~derive: Tilia.Core.deriver(store)=?, unit) => store;
  };

  module Make = (Schema: Schema) => {
    type state = Schema.state;
    type action = Schema.action;
    type t = Schema.store;
    type listener_id = StoreEvents.listener_id;
    type store_event = StoreEvents.store_event(action);
    type listener = StoreEvents.listener(action);

    type broadcast_channel;

    [@platform js]
    let openBroadcastChannel: string => broadcast_channel = [%raw
      {|
        function(name) {
          return new BroadcastChannel(name);
        }
      |}
    ];

    [@platform native]
    let openBroadcastChannel = (_name: string) => Obj.magic();

    [@platform js]
    let postBroadcastMessage: (broadcast_channel, string) => unit = [%raw
      {|
        function(channel, message) {
          channel.postMessage(message);
        }
      |}
    ];

    [@platform native]
    let postBroadcastMessage = (_channel, _message) => ();

    [@platform js]
    let setBroadcastHandler: (broadcast_channel, string => unit) => unit = [%raw
      {|
        function(channel, handler) {
          channel.onmessage = function(event) {
            handler(event.data);
          };
        }
      |}
    ];

    [@platform native]
    let setBroadcastHandler = (_channel, _handler) => ();

    let sourceRef: ref(option(StoreSource.actions(state))) = ref(None);
    let channelRef: ref(option(broadcast_channel)) = ref(None);
    let suppressPublishRef: ref(bool) = ref(false);
    let listenersRef: StoreEvents.registry(action) = ref([||]);

    let empty = Schema.makeStore(~state=Schema.emptyState, ());

    let createStore = (state: state) => Schema.makeStore(~state, ());

    let serializeState = (state: state) =>
      StoreJson.stringify(Schema.state_to_json, state);

    let serializeSnapshot = serializeState;

    let writeStateRecord = (state: state) =>
      switch%platform (Runtime.platform) {
      | Client =>
        let _ =
          StoreIndexedDB.setState(
            ~name=Schema.storeName,
            {
              scopeKey: Schema.scopeKeyOfState(state),
              value: Schema.state_to_json(state),
              timestamp: Schema.timestampOfState(state),
            },
          );
        ();
      | Server => ()
      };

    let broadcastState = (state: state) =>
      switch%platform (Runtime.platform) {
      | Client =>
        switch (channelRef.contents) {
        | Some(channel) =>
          postBroadcastMessage(
            channel,
            StoreJson.stringify(
              json => json,
              StoreJson.parse(
                "{\"scopeKey\":"
                ++ Melange_json.Primitives.string_to_json(
                     Schema.scopeKeyOfState(state),
                   )
                   ->Melange_json.to_string
                ++ ",\"timestamp\":"
                ++ Melange_json.Primitives.float_to_json(
                     Schema.timestampOfState(state),
                   )
                   ->Melange_json.to_string
                ++ ",\"state\":"
                ++ StoreJson.stringify(Schema.state_to_json, state)
                ++ "}",
              ),
            ),
          )
        | None => ()
        }
      | Server => ()
      };

    let setExternalState =
        (~actions: StoreSource.actions(state), nextState: state) => {
      suppressPublishRef := true;
      actions.set(nextState);
    };

    let persistState = (state: state) =>
      switch%platform (Runtime.platform) {
      | Client =>
        writeStateRecord(state);
        if (suppressPublishRef.contents) {
          suppressPublishRef := false;
        } else {
          broadcastState(state);
        };
        ();
      | Server => ()
      };

    let reconcilePersistedState = (actions: StoreSource.actions(state)) =>
      switch%platform (Runtime.platform) {
      | Client =>
        let currentState = actions.get();
        let _ =
          Js.Promise.then_(
            persistedState => {
              switch (persistedState) {
              | Some(record: StoreIndexedDB.state_record) =>
                if (record.timestamp > Schema.timestampOfState(actions.get())) {
                  setExternalState(
                    ~actions,
                    Schema.state_of_json(record.value),
                  );
                } else {
                  writeStateRecord(actions.get());
                }
              | None => writeStateRecord(actions.get())
              };
              Js.Promise.resolve();
            },
            StoreIndexedDB.getState(
              ~name=Schema.storeName,
              ~scopeKey=Schema.scopeKeyOfState(currentState),
              (),
            ),
          );
        ();
      | Server => ()
      };

    let hydrateStore = () => {
      let initialState =
        switch%platform (Runtime.platform) {
        | Client =>
          switch (
            Hydration.parseState(
              ~stateElementId=Schema.stateElementId,
              ~decodeState=Schema.state_of_json,
            )
          ) {
          | Some(state) => state
          | None => Schema.emptyState
          }
        | Server => Schema.emptyState
        };

      switch%platform (Runtime.platform) {
      | Client =>
        let source =
          StoreSource.make(
            ~afterSet=persistState,
            ~mount=
              actions => {
                sourceRef := Some(actions);
                let channel =
                  openBroadcastChannel("resync.store." ++ Schema.storeName);
                channelRef := Some(channel);
                setBroadcastHandler(channel, message => {
                  switch (StoreJson.tryParse(message)) {
                  | Some(json) =>
                    let scopeKey =
                      StoreJson.requiredField(
                        ~json,
                        ~fieldName="scopeKey",
                        ~decode=Melange_json.Primitives.string_of_json,
                      );
                    let timestamp =
                      StoreJson.requiredField(
                        ~json,
                        ~fieldName="timestamp",
                        ~decode=Melange_json.Primitives.float_of_json,
                      );
                    if (scopeKey == Schema.scopeKeyOfState(actions.get())
                        && timestamp > Schema.timestampOfState(actions.get())) {
                      let nextState =
                        StoreJson.requiredField(
                          ~json,
                          ~fieldName="state",
                          ~decode=Schema.state_of_json,
                        );
                      setExternalState(~actions, nextState);
                    };
                  | None => ()
                  }
                });
                reconcilePersistedState(actions);
              },
            initialState,
          );
        StoreComputed.make(
          ~client=
            derive => Schema.makeStore(~state=source.value, ~derive, ()),
          ~server=() => Schema.makeStore(~state=initialState, ()),
        );
      | Server => Schema.makeStore(~state=initialState, ())
      };
    };

    let dispatch = (action: action) =>
      switch (sourceRef.contents) {
      | Some(actions) =>
        actions.update(state => Schema.reduce(~state, ~action))
      | None => ()
      };

    module Events = {
      let listen = (listener: listener) =>
        StoreEvents.Events.listen(~registry=listenersRef, listener);

      let unlisten = (listenerId: listener_id) =>
        StoreEvents.Events.unlisten(~registry=listenersRef, listenerId);
    };

    module Context = {
      let context = React.createContext(empty);

      module Provider = {
        let makeProps = (~value, ~children, ()) => {
          "value": value,
          "children": children,
        };
        let make = React.Context.provider(context);
      };

      let useStore = () => React.useContext(context);
    };
  };
};

module Synced = {
  type broadcast_channel;

  [@platform js]
  let openBroadcastChannel: string => broadcast_channel = [%raw
    {|
      function(name) {
        return new BroadcastChannel(name);
      }
    |}
  ];

  [@platform native]
  let openBroadcastChannel = (_name: string) => Obj.magic();

  [@platform js]
  let postBroadcastMessage: (broadcast_channel, string) => unit = [%raw
    {|
      function(channel, message) {
        channel.postMessage(message);
      }
    |}
  ];

  [@platform native]
  let postBroadcastMessage = (_channel, _message) => ();

  [@platform js]
  let setBroadcastHandler: (broadcast_channel, string => unit) => unit = [%raw
    {|
      function(channel, handler) {
        channel.onmessage = function(event) {
          handler(event.data);
        };
      }
    |}
  ];

  [@platform native]
  let setBroadcastHandler = (_channel, _handler) => ();

  [@platform js]
  external setTimeout: (unit => unit, int) => int = "setTimeout";

  [@platform native]
  let setTimeout = (_callback, _timeout) => 0;

  [@platform js] external clearTimeout: int => unit = "clearTimeout";

  [@platform native]
  let clearTimeout = _id => ();

  [@platform js]
  external deleteTimer: (Js.Dict.t(int), string) => unit = "delete";

  [@platform native]
  let deleteTimer = (_dict, _key) => ();

  /* SyncController: centralized sync state and listener management.
     Owns connection state, ack timers, typed listener registry, and
     queued nested dispatch behavior. */
  module type SyncController = {
    type action;
    type store_event = StoreEvents.store_event(action);
    type listener = StoreEvents.listener(action);
    type listener_id = StoreEvents.listener_id;

    /* Connection state owned by controller */
    let getConnectionHandle: unit => option(RealtimeClient.Socket.connection_handle);
    let setConnectionHandle: option(RealtimeClient.Socket.connection_handle) => unit;
    let disposeConnectionHandle: unit => unit;
    let getHasOpened: unit => bool;
    let setHasOpened: bool => unit;

    /* Cross-tab broadcast channel */
    let getBroadcastChannel: unit => option(broadcast_channel);
    let setBroadcastChannel: option(broadcast_channel) => unit;

    /* Ack timers */
    let clearAckTimeout: string => unit;
    let scheduleAckTimeout: (string, unit => unit) => unit;

    /* Typed listener registry with queued dispatch */
    let subscribe: listener => listener_id;
    let unsubscribe: listener_id => unit;
    let emit: store_event => unit;

    /* Pending dispatch queue for nested dispatch safety */
    let queueDispatch: (unit => unit) => unit;
    let isEmitting: unit => bool;
  };

  module MakeSyncController = (Config: {type action;}) : (SyncController with type action = Config.action) => {
    type action = Config.action;
    type store_event = StoreEvents.store_event(action);
    type listener = StoreEvents.listener(action);
    type listener_id = StoreEvents.listener_id;

    /* Connection state */
    let connectionHandleRef: ref(option(RealtimeClient.Socket.connection_handle)) = ref(None);
    let hasOpenedRef: ref(bool) = ref(false);

    let getConnectionHandle = () => connectionHandleRef.contents;
    let setConnectionHandle = handle => connectionHandleRef := handle;

    let disposeConnectionHandle = () => {
      switch (connectionHandleRef.contents) {
      | Some(handle) =>
        RealtimeClient.Socket.disposeHandle(handle);
        connectionHandleRef := None;
      | None => ()
      };
    };

    let getHasOpened = () => hasOpenedRef.contents;
    let setHasOpened = value => hasOpenedRef := value;

    /* Cross-tab broadcast channel */
    let channelRef: ref(option(broadcast_channel)) = ref(None);
    let getBroadcastChannel = () => channelRef.contents;
    let setBroadcastChannel = channel => channelRef := channel;

    /* Ack timers */
    let timersRef: ref(Js.Dict.t(int)) = ref(Js.Dict.empty());

    let clearAckTimeout = actionId => {
      switch (timersRef.contents->Js.Dict.get(actionId)) {
      | Some(timerId) =>
        clearTimeout(timerId);
        deleteTimer(timersRef.contents, actionId);
      | None => ()
      };
    };

    let scheduleAckTimeout = (actionId, callback) => {
      clearAckTimeout(actionId);
      let timerId = setTimeout(callback, StoreActionLedger.ackTimeoutMs);
      timersRef.contents->Js.Dict.set(actionId, timerId);
    };

    /* Typed listener registry */
    let listenersRef: StoreEvents.registry(Config.action) = ref([||]);

    let subscribe = (listener: listener): listener_id => {
      let listenerId = UUID.make();
      listenersRef.contents =
        Js.Array.concat(~other=[|(listenerId, listener)|], listenersRef.contents);
      listenerId;
    };

    let unsubscribe = (listenerId: listener_id) => {
      listenersRef.contents =
        listenersRef.contents->Js.Array.filter(~f=((currentId, _listener)) =>
          currentId != listenerId
        );
    };

    /* Queued nested dispatch: when emit is active, dispatches are queued
       and drained after the current listener batch completes. This prevents
       reentrancy while preserving FIFO ordering. */
    let pendingDispatchesRef: ref(array(unit => unit)) = ref([||]);
    let isEmittingRef: ref(bool) = ref(false);

    let isEmitting = () => isEmittingRef.contents;

    let queueDispatch = (dispatchFn: unit => unit) => {
      pendingDispatchesRef.contents =
        Js.Array.concat(~other=[|dispatchFn|], pendingDispatchesRef.contents);
    };

    let drainPendingDispatches = () => {
      let toDrain = pendingDispatchesRef.contents;
      pendingDispatchesRef.contents = [||];
      toDrain->Js.Array.forEach(~f=fn => fn());
    };

    let emit = (event: store_event) => {
      /* Capture stable snapshot of listeners before iteration.
         listen/unlisten during emit affect subsequent batches only. */
      let snapshot = listenersRef.contents;
      isEmittingRef := true;

      snapshot->Js.Array.forEach(~f=((_, listener)) => listener(event));

      isEmittingRef := false;
      drainPendingDispatches();
    };
  };

  module type Schema = {
    type state;
    type action;
    type store;
    type subscription;
    type patch;

    let reduce: (~state: state, ~action: action) => state;
    let emptyState: state;
    let storeName: string;
    let stateElementId: string;
    let scopeKeyOfState: state => string;
    let timestampOfState: state => float;
    let setTimestamp: (~state: state, ~timestamp: float) => state;
    let state_of_json: StoreJson.json => state;
    let state_to_json: state => StoreJson.json;
    let action_of_json: StoreJson.json => action;
    let action_to_json: action => StoreJson.json;
    let makeStore:
      (~state: state, ~derive: Tilia.Core.deriver(store)=?, unit) => store;
    let subscriptionOfState: state => option(subscription);
    let encodeSubscription: subscription => string;
    let eventUrl: string;
    let baseUrl: string;
    let decodePatch: StoreJson.json => option(patch);
    let updateOfPatch: (patch, state) => state;
    /* Legacy compatibility hooks. New code should prefer Runtime.Events.listen and
       the narrow StoreEvents.store_event surface instead of raw per-frame callback
       registration. */
    let onActionError: string => unit;
    let onActionAck: option((~dispatch: action => unit, ~action: action, ~actionId: string) => unit);
    let onCustom: option(StoreJson.json => unit);
    let onMedia: option(StoreJson.json => unit);
    let onError: option((~dispatch: action => unit) => string => unit);
    let onOpen: option((~dispatch: action => unit) => unit);
    /* Optional hook called when a connection handle is created. Allows external
       code (e.g., video-chat media transport) to access the handle for sending
       raw frames without storing singleton state in RealtimeClient. */
    let onConnectionHandle: option(RealtimeClient.Socket.connection_handle => unit);
  };

  module Make = (Schema: Schema) => {
    type state = Schema.state;
    type action = Schema.action;
    type t = Schema.store;
    type listener_id = StoreEvents.listener_id;
    type store_event = StoreEvents.store_event(action);
    type listener = StoreEvents.listener(action);

    /* Hydration and source state stay outside the controller */
    let sourceRef: ref(option(StoreSource.actions(state))) = ref(None);
    let confirmedStateRef: ref(state) = ref(Schema.emptyState);
    let suppressPublishRef: ref(bool) = ref(false);

    /* Initialize the SyncController that owns connection state, timers,
       listeners, and queued dispatch behavior. */
    module Controller = MakeSyncController({type action = Schema.action;});

    let empty = Schema.makeStore(~state=Schema.emptyState, ());

    let createStore = (state: state) => Schema.makeStore(~state, ());

    let serializeState = (state: state) =>
      StoreJson.stringify(Schema.state_to_json, state);

    let serializeSnapshot = serializeState;

    let actionOptionOfRecord = record =>
      switch (record) {
      | Some(record: StoreActionLedger.t) =>
        Some(Schema.action_of_json(record.action))
      | None => None
      };

    /* Emit through the controller for stable snapshot and queued dispatch */
    let emitEvent = (event: store_event) => Controller.emit(event);

    let broadcastOptimisticAction = (record: StoreActionLedger.t) =>
      switch%platform (Runtime.platform) {
      | Client =>
        switch (Controller.getBroadcastChannel()) {
        | Some(channel) =>
          postBroadcastMessage(
            channel,
            StoreJson.stringify(
              json => json,
              StoreJson.parse(
                "{\"type\":\"optimistic_action\",\"scopeKey\":"
                ++ Melange_json.Primitives.string_to_json(record.scopeKey)
                   ->Melange_json.to_string
                ++ ",\"actionId\":"
                ++ Melange_json.Primitives.string_to_json(record.id)
                   ->Melange_json.to_string
                ++ "}",
              ),
            ),
          )
        | None => ()
        }
      | Server => ()
      };

    let broadcastConfirmedState = (state: state) =>
      switch%platform (Runtime.platform) {
      | Client =>
        switch (Controller.getBroadcastChannel()) {
        | Some(channel) =>
          postBroadcastMessage(
            channel,
            StoreJson.stringify(
              json => json,
              StoreJson.parse(
                "{\"type\":\"confirmed_state\",\"scopeKey\":"
                ++ Melange_json.Primitives.string_to_json(
                     Schema.scopeKeyOfState(state),
                   )
                   ->Melange_json.to_string
                ++ ",\"timestamp\":"
                ++ Melange_json.Primitives.float_to_json(
                     Schema.timestampOfState(state),
                   )
                   ->Melange_json.to_string
                ++ ",\"state\":"
                ++ StoreJson.stringify(Schema.state_to_json, state)
                ++ "}",
              ),
            ),
          )
        | None => ()
        }
      | Server => ()
      };

    let persistConfirmedState = (state: state) =>
      switch%platform (Runtime.platform) {
      | Client =>
        let _ =
          StoreIndexedDB.setState(
            ~name=Schema.storeName,
            {
              scopeKey: Schema.scopeKeyOfState(state),
              value: Schema.state_to_json(state),
              timestamp: Schema.timestampOfState(state),
            },
          );
        if (suppressPublishRef.contents) {
          suppressPublishRef := false;
        } else {
          broadcastConfirmedState(state);
        };
        ();
      | Server => ()
      };

    let replayActions = (~confirmed: state, ~records) => {
      let sorted = StoreActionLedger.sortByEnqueuedAt(records);
      let length = Array.length(sorted);
      let rec loop = (index, current) =>
        if (index >= length) {
          current;
        } else {
          let record: StoreActionLedger.t = sorted[index];
          loop(
            index + 1,
            Schema.reduce(
              ~state=current,
              ~action=Schema.action_of_json(record.action),
            ),
          );
        };
      loop(0, confirmed);
    };

    let rec refreshOptimisticState = () =>
      switch%platform (Runtime.platform) {
      | Client =>
        switch (sourceRef.contents) {
        | Some(actions) =>
          let confirmedState = confirmedStateRef.contents;
          let confirmedTimestamp = Schema.timestampOfState(confirmedState);
          let _ =
            Js.Promise.then_(
              records => {
                let idsToDelete =
                  records
                  ->Js.Array.filter(~f=(record: StoreActionLedger.t) =>
                      switch (StoreActionLedger.statusOfString(record.status)) {
                      | Acked =>
                        UUID.timestamp(record.id) <= confirmedTimestamp
                      | _ => false
                      }
                    )
                  ->Js.Array.map(~f=(record: StoreActionLedger.t) =>
                      record.id
                    );
                let remaining =
                  records->Js.Array.filter(~f=(record: StoreActionLedger.t) =>
                    switch (StoreActionLedger.statusOfString(record.status)) {
                    | Pending
                    | Syncing => true
                    | Acked => UUID.timestamp(record.id) > confirmedTimestamp
                    | Failed => false
                    }
                  );
                actions.set(
                  replayActions(
                    ~confirmed=confirmedState,
                    ~records=remaining,
                  ),
                );
                if (Array.length(idsToDelete) > 0) {
                  let _ =
                    StoreActionLedger.deleteByIds(
                      ~storeName=Schema.storeName,
                      ~ids=idsToDelete,
                      (),
                    );
                  ();
                };
                Js.Promise.resolve();
              },
              StoreActionLedger.getByScope(
                ~storeName=Schema.storeName,
                ~scopeKey=Schema.scopeKeyOfState(confirmedState),
                (),
              ),
            );
          ();
        | None => ()
        }
      | Server => ()
      }

    and applyExternalConfirmedState =
        (~actions: StoreSource.actions(state), nextState: state) => {
      suppressPublishRef := true;
      confirmedStateRef := nextState;
      actions.set(nextState);
      refreshOptimisticState();
    }

    and sendQueuedRecord = (record: StoreActionLedger.t) => {
      let sent =
        switch (Controller.getConnectionHandle()) {
        | Some(handle) =>
          RealtimeClient.Socket.sendAction(
            ~handle,
            ~actionId=record.id,
            ~action=record.action,
          )
        | None => false
        };
      let nextRecord =
        if (sent) {
          {
            ...record,
            status: StoreActionLedger.statusToString(Syncing),
          };
        } else {
          {
            ...record,
            status: StoreActionLedger.statusToString(Pending),
          };
        };
      let _ = StoreActionLedger.put(~storeName=Schema.storeName, nextRecord);
      if (sent) {
        Controller.scheduleAckTimeout(
          nextRecord.id,
          () => handleAckTimeout(nextRecord.id),
        );
      };
    }

    and handleAckTimeout = actionId =>
      switch%platform (Runtime.platform) {
      | Client =>
        let _ =
          Js.Promise.then_(
            current =>
              switch (current) {
              | Some(record: StoreActionLedger.t) =>
                switch (StoreActionLedger.statusOfString(record.status)) {
                | Acked
                | Failed => Js.Promise.resolve()
                | Pending
                | Syncing =>
                  if (record.retryCount >= StoreActionLedger.maxRetries) {
                    let message = "Timed out waiting for acknowledgement";
                    Js.Promise.then_(
                      _ => {
                        refreshOptimisticState();
                        /* Failure ordering contract: the action ledger status is
                           updated before the public ActionFailed event and any
                           legacy callback fire. */
                        emitEvent(
                          StoreEvents.ActionFailed({
                            actionId,
                            action: Some(Schema.action_of_json(record.action)),
                            message,
                          }),
                        );
                        Schema.onActionError(message);
                        Js.Promise.resolve();
                      },
                      StoreActionLedger.updateStatus(
                        ~storeName=Schema.storeName,
                        ~id=actionId,
                        ~status=Failed,
                        ~error=message,
                        (),
                      ),
                    );
                  } else {
                    let nextRecord = {
                      ...record,
                      retryCount: record.retryCount + 1,
                    };
                    sendQueuedRecord(nextRecord);
                    Js.Promise.resolve();
                  }
                }
              | None => Js.Promise.resolve()
              },
            StoreActionLedger.get(
              ~storeName=Schema.storeName,
              ~id=actionId,
              (),
            ),
          );
        ();
      | Server => ()
      };

    /* Core dispatch implementation with ledger persistence and optimistic broadcast.
       This is the actual workhorse that executes the dispatch. */
    let rec executeDispatch = (action: action) =>
      switch (sourceRef.contents) {
      | Some(actions) =>
        let currentState = actions.get();
        let nextState = Schema.reduce(~state=currentState, ~action);
        let actionId = UUID.make();
        let record =
          StoreActionLedger.make(
            ~id=actionId,
            ~scopeKey=Schema.scopeKeyOfState(nextState),
            ~action=Schema.action_to_json(action),
            (),
          );
        actions.set(nextState);
        let _ =
          Js.Promise.then_(
            _ => {
              broadcastOptimisticAction(record);
              sendQueuedRecord(record);
              Js.Promise.resolve();
            },
            StoreActionLedger.put(~storeName=Schema.storeName, record),
          )
          |> Js.Promise.catch(_ => {
               sendQueuedRecord(record);
               Js.Promise.resolve();
             });
        ();
      | None => ()
      }

    /* Public dispatch that respects controller emission gating.
       When emitted during an active listener batch, dispatch is queued
       and executed after the batch completes. This prevents reentrancy
       while preserving FIFO ordering for both typed listeners and legacy callbacks. */
    and dispatch = (action: action) => {
      if (Controller.isEmitting()) {
        Controller.queueDispatch(() => executeDispatch(action));
      } else {
        executeDispatch(action);
      }
    }

    /* Legacy callback dispatch wrapper - calls the main dispatch
       since both respect emission gating uniformly. */
    and safeDispatch = (action: action) => dispatch(action);

    let resumePendingActions = () =>
      switch%platform (Runtime.platform) {
      | Client =>
        let _ =
          Js.Promise.then_(
            records => {
              StoreActionLedger.sortByEnqueuedAt(records)
              ->Js.Array.forEach(~f=(record: StoreActionLedger.t) =>
                  switch (StoreActionLedger.statusOfString(record.status)) {
                  | Pending
                  | Syncing => sendQueuedRecord(record)
                  | Acked
                  | Failed => ()
                  }
                );
              Js.Promise.resolve();
            },
            StoreActionLedger.getByScope(
              ~storeName=Schema.storeName,
              ~scopeKey=Schema.scopeKeyOfState(confirmedStateRef.contents),
              (),
            ),
          );
        ();
      | Server => ()
      };

    let handleAck = (actionId: string, status: string, error: option(string)) => {
      Controller.clearAckTimeout(actionId);
      switch (status) {
      | "ok" =>
        let _ =
          Js.Promise.then_(
            record => {
              Js.Promise.then_(
                _ => {
                  /* Ack ordering contract: the action ledger status is updated
                     before ActionAcked listeners or legacy callbacks run. */
                  emitEvent(
                    StoreEvents.ActionAcked({
                      actionId,
                      action: actionOptionOfRecord(record),
                    }),
                  );
                  switch (record) {
                  | Some(r: StoreActionLedger.t) =>
                    switch (Schema.onActionAck) {
                    | Some(onActionAck) =>
                      onActionAck(
                        ~dispatch=safeDispatch,
                        ~action=Schema.action_of_json(r.action),
                        ~actionId,
                      )
                    | None => ()
                    }
                  | None => ()
                  };
                  Js.Promise.resolve();
                },
                StoreActionLedger.updateStatus(
                  ~storeName=Schema.storeName,
                  ~id=actionId,
                  ~status=Acked,
                  (),
                ),
              );
            },
            StoreActionLedger.get(
              ~storeName=Schema.storeName,
              ~id=actionId,
              (),
            ),
          );
        ();
      | "error" =>
        let message =
          switch (error) {
          | Some(message) => message
          | None => "Mutation failed"
          };
        let _ =
          Js.Promise.then_(
            record => {
              Js.Promise.then_(
                _ => {
                  refreshOptimisticState();
                  /* Failure ordering contract: emit ActionFailed only after the
                     ledger status reflects the failed action. */
                  emitEvent(
                    StoreEvents.ActionFailed({
                      actionId,
                      action: actionOptionOfRecord(record),
                      message,
                    }),
                  );
                  Schema.onActionError(message);
                  Js.Promise.resolve();
                },
                StoreActionLedger.updateStatus(
                  ~storeName=Schema.storeName,
                  ~id=actionId,
                  ~status=Failed,
                  ~error?,
                  (),
                ),
              );
            },
            StoreActionLedger.get(
              ~storeName=Schema.storeName,
              ~id=actionId,
              (),
            ),
          );
        ();
      | _ => ()
      };
    };

    let handleSnapshot = (snapshotJson: StoreJson.json) => {
      let snapshotState = Schema.state_of_json(snapshotJson);
      /* Ordering contract: snapshot application, persistence, and optimistic
         replay complete before any future snapshot-adjacent events fire. Raw
         snapshot transport frames are intentionally not part of the public
         event surface. */
      confirmedStateRef := snapshotState;
      persistConfirmedState(snapshotState);
      refreshOptimisticState();
    };

    let handlePatch = (~payload: StoreJson.json, ~timestamp: float) => {
      switch (Schema.decodePatch(payload)) {
      | Some(patch) =>
        let nextConfirmedState =
          Schema.setTimestamp(
            ~state=Schema.updateOfPatch(patch, confirmedStateRef.contents),
            ~timestamp,
          );
        /* Ordering contract: patch application, persistence, and optimistic
           replay complete before any patch-adjacent events fire. Raw patch
           frames stay internal to the synced runtime. */
        confirmedStateRef := nextConfirmedState;
        persistConfirmedState(nextConfirmedState);
        refreshOptimisticState();
      | None => ()
      };
    };

    let startSubscription = state =>
      switch (Schema.subscriptionOfState(state)) {
      | Some(subscription) =>
        {
          Controller.disposeConnectionHandle();
          let handle =
            RealtimeClient.Socket.subscribeSynced(
              ~subscription=Schema.encodeSubscription(subscription),
              ~updatedAt=Schema.timestampOfState(state),
              ~onOpen=() => {
                let lifecycleEvent =
                  if (Controller.getHasOpened()) {
                    StoreEvents.Reconnect;
                  } else {
                    Controller.setHasOpened(true);
                    StoreEvents.Open;
                  };
                /* Lifecycle events originate from the store-owned sync runtime instead
                   of demo code so reconnect/open ordering stays centralized. */
                emitEvent(lifecycleEvent);
                switch (Schema.onOpen) {
                | Some(onOpen) => onOpen(~dispatch=safeDispatch)
                | None => ()
                };
                resumePendingActions();
              },
              ~onClose=() => emitEvent(StoreEvents.Close),
              ~onPatch=handlePatch,
              ~onCustom=json => {
                emitEvent(StoreEvents.CustomEvent(json));
                switch (Schema.onCustom) {
                | Some(onCustom) => onCustom(json)
                | None => ()
                };
              },
              ~onMedia=json => {
                emitEvent(StoreEvents.MediaEvent(json));
                switch (Schema.onMedia) {
                | Some(onMedia) => onMedia(json)
                | None => ()
                };
              },
              ~onError=message => {
                /* Connection errors are surfaced from the runtime-owned sync layer, not
                   by ad hoc demo-owned socket hooks. */
                emitEvent(StoreEvents.ConnectionError(message));
                switch (Schema.onError) {
                | Some(onError) => onError(~dispatch=safeDispatch)(message)
                | None => ()
                };
              },
              ~onSnapshot=handleSnapshot,
              ~onAck=handleAck,
              ~eventUrl=Schema.eventUrl,
              ~baseUrl=Schema.baseUrl,
              (),
            );
          Controller.setConnectionHandle(Some(handle));
          switch (Schema.onConnectionHandle) {
          | Some(callback) => callback(handle)
          | None => ()
          };
        }
      | None => Controller.disposeConnectionHandle()
      };

    let hydrateStore = () => {
      let initialState =
        switch%platform (Runtime.platform) {
        | Client =>
          switch (
            Hydration.parseState(
              ~stateElementId=Schema.stateElementId,
              ~decodeState=Schema.state_of_json,
            )
          ) {
          | Some(state) => state
          | None => Schema.emptyState
          }
        | Server => Schema.emptyState
        };

      confirmedStateRef := initialState;
      Controller.setHasOpened(false);
      Controller.disposeConnectionHandle();

      switch%platform (Runtime.platform) {
      | Client =>
        let source =
          StoreSource.make(
            ~afterSet=_next => (),
            ~mount=
              actions => {
                sourceRef := Some(actions);
                let channel =
                  openBroadcastChannel("resync.store." ++ Schema.storeName);
                Controller.setBroadcastChannel(Some(channel));
                setBroadcastHandler(channel, message => {
                  switch (StoreJson.tryParse(message)) {
                  | Some(json) =>
                    let messageType =
                      StoreJson.optionalField(
                        ~json,
                        ~fieldName="type",
                        ~decode=Melange_json.Primitives.string_of_json,
                      );
                    let scopeKey =
                      StoreJson.requiredField(
                        ~json,
                        ~fieldName="scopeKey",
                        ~decode=Melange_json.Primitives.string_of_json,
                      );
                    let currentConfirmedState = confirmedStateRef.contents;
                    if (scopeKey
                        == Schema.scopeKeyOfState(currentConfirmedState)) {
                      switch (messageType) {
                      | Some("optimistic_action") => refreshOptimisticState()
                      | _ =>
                        let timestamp =
                          StoreJson.requiredField(
                            ~json,
                            ~fieldName="timestamp",
                            ~decode=Melange_json.Primitives.float_of_json,
                          );
                        if (timestamp
                            > Schema.timestampOfState(currentConfirmedState)) {
                          let nextState =
                            StoreJson.requiredField(
                              ~json,
                              ~fieldName="state",
                              ~decode=Schema.state_of_json,
                            );
                          applyExternalConfirmedState(~actions, nextState);
                        };
                      };
                    };
                  | None => ()
                  }
                });
                let _ =
                  Js.Promise.then_(
                    persistedState => {
                      let baseState =
                        switch (persistedState) {
                        | Some(record: StoreIndexedDB.state_record) =>
                          if (record.timestamp
                              > Schema.timestampOfState(initialState)) {
                            Schema.state_of_json(record.value);
                          } else {
                            initialState;
                          }
                        | None => initialState
                        };
                      confirmedStateRef := baseState;
                      persistConfirmedState(baseState);
                      actions.set(baseState);
                      refreshOptimisticState();
                      startSubscription(baseState);
                      Js.Promise.resolve();
                    },
                    StoreIndexedDB.getState(
                      ~name=Schema.storeName,
                      ~scopeKey=Schema.scopeKeyOfState(initialState),
                      (),
                    ),
                  );
                ();
              },
            initialState,
          );
        StoreComputed.make(
          ~client=
            derive => Schema.makeStore(~state=source.value, ~derive, ()),
          ~server=() => Schema.makeStore(~state=initialState, ()),
        );
      | Server => Schema.makeStore(~state=initialState, ())
      };
    };

    module Events = {
      let listen = (listener: listener) => Controller.subscribe(listener);

      let unlisten = (listenerId: listener_id) => Controller.unsubscribe(listenerId);
    };

    module Context = {
      let context = React.createContext(empty);

      module Provider = {
        let makeProps = (~value, ~children, ()) => {
          "value": value,
          "children": children,
        };
        let make = React.Context.provider(context);
      };

      let useStore = () => React.useContext(context);
    };
  };
};