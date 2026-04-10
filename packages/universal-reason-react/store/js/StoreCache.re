type state_record('state) = {
  scopeKey: string,
  state: 'state,
  timestamp: float,
};

type action_record('action) = {
  id: string,
  scopeKey: string,
  action: 'action,
  status: string,
  enqueuedAt: float,
  retryCount: int,
  error: option(string),
};

module type Adapter = {
  type state;
  type action;

  let getState:
    (~storeName: string, ~scopeKey: string, unit) =>
    Js.Promise.t(option(state_record(state)));
  let setState: (~storeName: string, state_record(state)) => Js.Promise.t(unit);

  let getAction:
    (~storeName: string, ~id: string, unit) =>
    Js.Promise.t(option(action_record(action)));
  let putAction:
    (~storeName: string, action_record(action)) => Js.Promise.t(unit);
  let getActionsByScope:
    (~storeName: string, ~scopeKey: string, unit) =>
    Js.Promise.t(array(action_record(action)));
  let deleteActions:
    (~storeName: string, ~ids: array(string), unit) => Js.Promise.t(unit);
};

module IndexedDB =
    (Schema: {
      type state;
      type action;
      let storeName: string;
      let scopeKeyOfState: state => string;
      let timestampOfState: state => float;
      let state_of_json: StoreJson.json => state;
      let state_to_json: state => StoreJson.json;
      let action_of_json: StoreJson.json => action;
      let action_to_json: action => StoreJson.json;
    })
    : (Adapter with type state = Schema.state and type action = Schema.action) => {
  type state = Schema.state;
  type action = Schema.action;

  /* Reference Schema values that are part of the interface contract
     but not directly used in the Adapter method implementations. */
  let _ =
    (Schema.storeName, Schema.scopeKeyOfState, Schema.timestampOfState);

  let getState = (~storeName, ~scopeKey, ()) =>
    Js.Promise.then_(
      result =>
        Js.Promise.resolve(
          Option.map(
            (record: StoreIndexedDB.state_record) => {
              scopeKey: record.scopeKey,
              state: Schema.state_of_json(record.value),
              timestamp: record.timestamp,
            },
            result,
          ),
        ),
      StoreIndexedDB.getState(~name=storeName, ~scopeKey, ()),
    );

  let setState = (~storeName, (record: state_record(state))) =>
    StoreIndexedDB.setState(
      ~name=storeName,
      {
        scopeKey: record.scopeKey,
        value: Schema.state_to_json(record.state),
        timestamp: record.timestamp,
      },
    );

  let mapActionRecord =
      (record: StoreIndexedDB.action_record): action_record(action) => {
    id: record.id,
    scopeKey: record.scopeKey,
    action: Schema.action_of_json(record.action),
    status: record.status,
    enqueuedAt: record.enqueuedAt,
    retryCount: record.retryCount,
    error: record.error,
  };

  let getAction = (~storeName, ~id, ()) =>
    Js.Promise.then_(
      result => Js.Promise.resolve(Option.map(mapActionRecord, result)),
      StoreIndexedDB.getAction(~name=storeName, ~id, ()),
    );

  let putAction = (~storeName, record) =>
    StoreIndexedDB.putAction(
      ~name=storeName,
      {
        id: record.id,
        scopeKey: record.scopeKey,
        action: Schema.action_to_json(record.action),
        status: record.status,
        enqueuedAt: record.enqueuedAt,
        retryCount: record.retryCount,
        error: record.error,
      },
    );

  let getActionsByScope = (~storeName, ~scopeKey, ()) =>
    Js.Promise.then_(
      results => Js.Promise.resolve(Array.map(mapActionRecord, results)),
      StoreIndexedDB.getActionsByScope(~name=storeName, ~scopeKey, ()),
    );

  let deleteActions = (~storeName, ~ids, ()) =>
    StoreIndexedDB.deleteActions(~name=storeName, ~ids, ());
};

module NoCache =
    (Schema: {
      type state;
      type action;
    })
    : (Adapter with type state = Schema.state and type action = Schema.action) => {
  type state = Schema.state;
  type action = Schema.action;

  let getState = (~storeName as _, ~scopeKey as _, ()) => Js.Promise.resolve(None);
  let setState = (~storeName as _, _record) => Js.Promise.resolve();
  let getAction = (~storeName as _, ~id as _, ()) => Js.Promise.resolve(None);
  let putAction = (~storeName as _, _record) => Js.Promise.resolve();
  let getActionsByScope = (~storeName as _, ~scopeKey as _, ()) =>
    Js.Promise.resolve([||]);
  let deleteActions = (~storeName as _, ~ids as _, ()) => Js.Promise.resolve();
};
