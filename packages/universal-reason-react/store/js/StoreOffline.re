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
let onActionError: string => unit;
let onActionAck: option((~dispatch: action => unit, ~action: action, ~actionId: string) => unit);
let onCustom: option(StoreJson.json => unit);
let onMedia: option(StoreJson.json => unit);
let onError: option((~dispatch: action => unit) => string => unit);
let onOpen: option((~dispatch: action => unit) => unit);
};

  module Make = (Schema: Schema) => {
    type state = Schema.state;
    type action = Schema.action;
    type t = Schema.store;

    let sourceRef: ref(option(StoreSource.actions(state))) = ref(None);
    let confirmedStateRef: ref(state) = ref(Schema.emptyState);
    let timersRef: ref(Js.Dict.t(int)) = ref(Js.Dict.empty());
    let channelRef: ref(option(broadcast_channel)) = ref(None);
    let suppressPublishRef: ref(bool) = ref(false);

    let empty = Schema.makeStore(~state=Schema.emptyState, ());

    let createStore = (state: state) => Schema.makeStore(~state, ());

    let serializeState = (state: state) =>
      StoreJson.stringify(Schema.state_to_json, state);

    let serializeSnapshot = serializeState;

    let broadcastOptimisticAction = (record: StoreActionLedger.t) =>
      switch%platform (Runtime.platform) {
      | Client =>
        switch (channelRef.contents) {
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
        switch (channelRef.contents) {
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

    let clearAckTimeout = actionId => {
      switch (timersRef.contents->Js.Dict.get(actionId)) {
      | Some(timerId) =>
        clearTimeout(timerId);
        deleteTimer(timersRef.contents, actionId);
      | None => ()
      };
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

    and scheduleAckTimeout = actionId =>
      switch%platform (Runtime.platform) {
      | Client =>
        clearAckTimeout(actionId);
        let timerId =
          setTimeout(
            () => handleAckTimeout(actionId),
            StoreActionLedger.ackTimeoutMs,
          );
        timersRef.contents->Js.Dict.set(actionId, timerId);
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
        RealtimeClient.Socket.sendAction(
          ~actionId=record.id,
          ~action=record.action,
        );
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
        scheduleAckTimeout(nextRecord.id);
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
                    Schema.onActionError(
                      "Timed out waiting for acknowledgement",
                    );
                    Js.Promise.then_(
                      _ => {
                        refreshOptimisticState();
                        Js.Promise.resolve();
                      },
                      StoreActionLedger.updateStatus(
                        ~storeName=Schema.storeName,
                        ~id=actionId,
                        ~status=Failed,
                        ~error="Timed out waiting for acknowledgement",
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

    let dispatch = (action: action) =>
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
      };

    let handleAck = (actionId: string, status: string, error: option(string)) => {
      clearAckTimeout(actionId);
      switch (status) {
      | "ok" =>
        let _ =
          Js.Promise.then_(
            record => {
              switch (record) {
              | Some(r: StoreActionLedger.t) =>
                let _ =
                  StoreActionLedger.updateStatus(
                    ~storeName=Schema.storeName,
                    ~id=actionId,
                    ~status=Acked,
                    (),
                  );
                switch (Schema.onActionAck) {
                | Some(onActionAck) =>
                  onActionAck(
                    ~dispatch,
                    ~action=Schema.action_of_json(r.action),
                    ~actionId,
                  )
                | None => ()
                };
              | None => ()
              };
              Js.Promise.resolve();
            },
            StoreActionLedger.get(
              ~storeName=Schema.storeName,
              ~id=actionId,
              (),
            ),
          );
        ();
      | "error" =>
        Schema.onActionError(
          switch (error) {
          | Some(message) => message
          | None => "Mutation failed"
          },
        );
        let _ =
          Js.Promise.then_(
            _ => {
              refreshOptimisticState();
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
        ();
      | _ => ()
      };
    };

    let handleSnapshot = (snapshotJson: StoreJson.json) => {
      let snapshotState = Schema.state_of_json(snapshotJson);
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
        confirmedStateRef := nextConfirmedState;
        persistConfirmedState(nextConfirmedState);
        refreshOptimisticState();
      | None => ()
      };
    };

let startSubscription = state =>
switch (Schema.subscriptionOfState(state)) {
| Some(subscription) =>
  RealtimeClient.Socket.subscribeSynced(
    ~subscription=Schema.encodeSubscription(subscription),
    ~updatedAt=Schema.timestampOfState(state),
    ~onOpen=() => {
      switch (Schema.onOpen) {
      | Some(onOpen) => onOpen(~dispatch)
      | None => ()
      };
      resumePendingActions();
    },
    ~onPatch=handlePatch,
    ~onCustom=
      switch (Schema.onCustom) {
      | Some(onCustom) => onCustom
      | None => ((_: Melange_json.t) => ())
      },
    ~onMedia=
      switch (Schema.onMedia) {
      | Some(onMedia) => onMedia
      | None => ((_: Melange_json.t) => ())
      },
    ~onError=
      switch (Schema.onError) {
      | Some(onError) => onError(~dispatch)
      | None => (_ => ())
      },
    ~onSnapshot=handleSnapshot,
    ~onAck=handleAck,
    ~eventUrl=Schema.eventUrl,
    ~baseUrl=Schema.baseUrl,
    (),
  )
  ->ignore
| None => ()
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
                channelRef := Some(channel);
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
