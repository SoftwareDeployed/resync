let derived = (~derive: option(Tilia.Core.deriver('store))=?, ~client, ~server, ()) =>
  switch (derive) {
  | Some(derive) => derive.derived(client)
  | None => server()
  };

let current = (~derive: option(Tilia.Core.deriver('store))=?, ~client, ~server, ()) =>
  switch (derive) {
  | Some(_) => client
  | None => server()
  };

let projected = (
  ~derive: option(Tilia.Core.deriver('store))=?,
  ~project,
  ~serverSource,
  ~fromStore,
  ~select,
  (),
) =>
  derived(
    ~derive?,
    ~client=store => select(project(fromStore(store))),
    ~server=() => select(project(serverSource)),
    (),
  );

/* ============================================================================
   Selector Helpers

   These helpers reduce boilerplate for common selector/projection patterns.
   They wrap the core current/derived/projected functions with convenient
   pre-bound configurations.
   ============================================================================ */

module Selectors = {
  /* Passthrough selector - returns the value as-is on both client and server.
     Useful for simple state fields that don't need transformation. */
  let passthrough = (~derive: option(Tilia.Core.deriver('store))=?, ~value, ()) =>
    current(~derive?, ~client=value, ~server=() => value, ());

  /* Client-only selector - uses a default on server, real value on client.
     Useful for client-specific state like UI toggle flags. */
  let clientOnly =
      (~derive: option(Tilia.Core.deriver('store))=?, ~client, ~serverDefault, ()) =>
    current(~derive?, ~client, ~server=() => serverDefault, ());

  /* Array length derived - common pattern for computing counts from arrays.
     Reduces repeated ~client/~server boilerplate for simple length calculations. */
  let arrayLength =
      (~derive: option(Tilia.Core.deriver('store))=?, ~getArray: 'store => array('a), ()) =>
    derived(
      ~derive?,
      ~client=store => getArray(store)->Array.length,
      ~server=() => 0,
      (),
    );

  /* Filtered count derived - counts items matching a predicate.
     Common for "completed count", "active count", etc. */
  let filteredCount =
      (
        ~derive: option(Tilia.Core.deriver('store))=?,
        ~getArray: 'store => array('a),
        ~predicate: 'a => bool,
        (),
      ) =>
    derived(
      ~derive?,
      ~client=store => getArray(store)->Js.Array.filter(~f=predicate)->Array.length,
      ~server=() => 0,
      (),
    );

  /* Nested field projection - extracts a nested field with proper SSR/hydration handling.
     Reduces repetitive ~project/~fromStore/~select wiring. */
  let field =
      (
        ~derive: option(Tilia.Core.deriver('store))=?,
        ~serverSource: 'state,
        ~fromStore: 'store => 'state,
        ~getField: 'state => 'field,
        (),
      ) =>
    projected(
      ~derive?,
      ~project=state => state,
      ~serverSource,
      ~fromStore,
      ~select=getField,
      (),
    );

  /* Computed projection - applies a transform function to state.
     The transform runs on projected state on both client and server. */
  let computed =
      (
        ~derive: option(Tilia.Core.deriver('store))=?,
        ~serverSource: 'state,
        ~fromStore: 'store => 'state,
        ~compute: 'state => 'result,
        (),
      ) =>
    projected(
      ~derive?,
      ~project=state => state,
      ~serverSource,
      ~fromStore,
      ~select=compute,
      (),
    );
};

/* ============================================================================
   Bootstrap Helpers

   These helpers reduce boilerplate for store hydration and provider wiring.
   They preserve control of root mounting (hydrateRoot vs createRoot) in
   app entrypoints while collapsing the repeated provider ceremony.

   Note: These are client-only helpers since they deal with hydration.
   ============================================================================ */

[@platform js]
module Bootstrap = {
  type hydratedResult('store) = {
    store: 'store,
    element: React.element,
  };

  type createdResult('store) = {
    store: 'store,
    element: React.element,
  };

  type multiHydratedResult('store) = {
    stores: array('store),
    element: React.element,
  };

  let withHydratedProvider = (~hydrateStore, ~provider, ~children) => {
    let store = hydrateStore();
    let element =
      React.createElement(
        provider,
        {"value": store, "children": children},
      );
    {store, element};
  };

  let withHydratedProviders = (~stores, ~children) => {
    let hydratedStores = stores->Js.Array.map(~f=((_, hydrate, _)) => hydrate());
    let element =
      stores
      ->Js.Array.reducei(
          ~init=children,
          ~f=(acc, (_, _, provider), index) => {
            let store = hydratedStores[index];
            React.createElement(
              provider,
              {"value": store, "children": acc},
            );
          },
        );
    {stores: hydratedStores, element};
  };

  let withCreatedProvider = (~createStore, ~provider, ~initialState, ~children) => {
    let store = createStore(initialState);
    let element =
      React.createElement(
        provider,
        {"value": store, "children": children},
      );
    {store, element};
  };
};

[@platform native]
module Bootstrap = {
  type hydratedResult('store) = {
    store: 'store,
    element: React.element,
  };

  type createdResult('store) = {
    store: 'store,
    element: React.element,
  };

  type multiHydratedResult('store) = {
    stores: array('store),
    element: React.element,
  };

  let withHydratedProvider = (~hydrateStore as _, ~provider as _, ~children as _) => {
    {store: Obj.magic(), element: React.null};
  };

  let withHydratedProviders = (~stores as _, ~children as _) => {
    {stores: [||], element: React.null};
  };

  let withCreatedProvider = (~createStore as _, ~provider as _, ~initialState as _, ~children as _) => {
    {store: Obj.magic(), element: React.null};
  };
};

type listener_id = StoreEvents.listener_id;
type listener('action) = StoreEvents.listener('action);
type store_event('action) = StoreEvents.store_event('action);

/* ============================================================================
   GuardTree
   ============================================================================ */

module GuardTree = StoreGuardTree;

/* ============================================================================
   Pipeline Builder Types
   ============================================================================ */

type schema('state, 'action, 'store) = {
  emptyState: 'state,
  reduce: (~state: 'state, ~action: 'action) => 'state,
  makeStore: (~state: 'state, ~derive: Tilia.Core.deriver('store)=?, unit) => 'store,
};

type json('state, 'action) = {
  state_of_json: StoreJson.json => 'state,
  state_to_json: 'state => StoreJson.json,
  action_of_json: StoreJson.json => 'action,
  action_to_json: 'action => StoreJson.json,
};

type queriesConfig('state) = StoreOffline.Local.queriesConfig('state);

type localPersistence('state) = {
  storeName: string,
  scopeKeyOfState: 'state => string,
  timestampOfState: 'state => float,
  stateElementId: option(string),
};

module Sync = {
  type transportConfig('state, 'subscription) = {
    subscriptionOfState: 'state => option('subscription),
    encodeSubscription: 'subscription => string,
    eventUrl: string,
    baseUrl: string,
  };

  type hooks('action) = {
    onActionError: option(string => unit),
    onActionAck:
      option((~dispatch: 'action => unit, ~action: 'action, ~actionId: string) => unit),
    onCustom: option(StoreJson.json => unit),
    onMedia: option(StoreJson.json => unit),
    onError: option((~dispatch: 'action => unit) => string => unit),
    onOpen: option((~dispatch: 'action => unit) => unit),
    onMultiplexedHandle: option(RealtimeClientMultiplexed.Multiplexed.t => unit),
  };

  type crudConfig('row, 'state) = {
    table: string,
    decodeRow: StoreJson.json => 'row,
    getId: 'row => string,
    getItems: 'state => array('row),
    setItems: ('state, array('row)) => 'state,
  };

  type customStrategy('state, 'patch) = {
    decodePatch: StoreJson.json => option('patch),
    updateOfPatch: ('patch, 'state) => 'state,
  };

  type crudStrategy('state, 'row) = {
    crud: crudConfig('row, 'state),
  };

  let defaultHooks = (): hooks('action) => {
    onActionError: None,
    onActionAck: None,
    onCustom: None,
    onMedia: None,
    onError: None,
    onOpen: None,
    onMultiplexedHandle: None,
  };

  let defaultOnActionError = (_message: string) => ();

  let custom = (~decodePatch, ~updateOfPatch): customStrategy('state, 'patch) => {
    decodePatch,
    updateOfPatch,
  };

  let crud = (~table, ~decodeRow, ~getId, ~getItems, ~setItems): crudStrategy('state, 'row) => {
    crud: {
      table,
      decodeRow,
      getId,
      getItems,
      setItems,
    },
  };
};

type syncPersistence('state, 'subscription) = {
  storeName: string,
  scopeKeyOfState: 'state => string,
  timestampOfState: 'state => float,
  stateElementId: option(string),
  setTimestamp: (~state: 'state, ~timestamp: float) => 'state,
  transport: Sync.transportConfig('state, 'subscription),
};

type local_input('state, 'action, 'store) = {
  schema: schema('state, 'action, 'store),
  json: json('state, 'action),
  guardTree: option(GuardTree.t('state, 'action)),
  queries: option(queriesConfig('state)),
  persistence: localPersistence('state),
};

type synced_input('state, 'action, 'store, 'subscription, 'patch, 'stream_event, 'streaming_state) = {
  schema: schema('state, 'action, 'store),
  json: json('state, 'action),
  guardTree: option(GuardTree.t('state, 'action)),
  queries: option(queriesConfig('state)),
  persistence: syncPersistence('state, 'subscription),
  hooks: Sync.hooks('action),
  strategy: Sync.customStrategy('state, 'patch),
  streams: option(StoreRuntimeTypes.streamsConfig('patch, 'stream_event, 'streaming_state)),
};

type synced_crud_input('state, 'action, 'store, 'subscription, 'row) = {
  schema: schema('state, 'action, 'store),
  json: json('state, 'action),
  guardTree: option(GuardTree.t('state, 'action)),
  queries: option(queriesConfig('state)),
  persistence: syncPersistence('state, 'subscription),
  hooks: Sync.hooks('action),
  crud: Sync.crudConfig('row, 'state),
};

/* ============================================================================
   Pipeline Builder Functions
   ============================================================================ */

type schemaBuilder('state, 'action, 'store) = {
  schema: schema('state, 'action, 'store),
  guardTree: option(GuardTree.t('state, 'action)),
};

type jsonBuilder('state, 'action, 'store) = {
  schema: schema('state, 'action, 'store),
  json: json('state, 'action),
  guardTree: option(GuardTree.t('state, 'action)),
};

type queriesBuilder('state, 'action, 'store) = {
  schema: schema('state, 'action, 'store),
  json: json('state, 'action),
  guardTree: option(GuardTree.t('state, 'action)),
  queries: option(queriesConfig('state)),
};

let make = () => {
  schema: {
    emptyState: (),
    reduce: (~state as _, ~action as _) => (),
    makeStore: (~state as _, ~derive as _=?, ()) => ()
  },
  guardTree: None,
};

type schemaConfig('state, 'action, 'store) = {
  emptyState: 'state,
  reduce: (~state: 'state, ~action: 'action) => 'state,
  makeStore: (~state: 'state, ~derive: Tilia.Core.deriver('store)=?, unit) => 'store,
};

let withSchema = (
  config: schemaConfig('state, 'action, 'store),
  _,
): schemaBuilder('state, 'action, 'store) => {
  schema: {
    emptyState: config.emptyState,
    reduce: config.reduce,
    makeStore: config.makeStore,
  },
  guardTree: None,
};

let withGuardTree = (
  ~guardTree: GuardTree.t('state, 'action),
  builder: schemaBuilder('state, 'action, 'store),
): schemaBuilder('state, 'action, 'store) => {
  ...builder,
  guardTree: Some(guardTree),
};

let withJson = (
  ~state_of_json: StoreJson.json => 'state,
  ~state_to_json: 'state => StoreJson.json,
  ~action_of_json: StoreJson.json => 'action,
  ~action_to_json: 'action => StoreJson.json,
  builder: schemaBuilder('state, 'action, 'store),
): queriesBuilder('state, 'action, 'store) => {
  schema: builder.schema,
  json: {state_of_json, state_to_json, action_of_json, action_to_json},
  guardTree: builder.guardTree,
  queries: None,
};

let withQueries = (
  ~applyQueryResult: (~state: 'state, ~channel: string, ~rows: array(StoreJson.json)) => 'state,
  builder: queriesBuilder('state, 'action, 'store),
): queriesBuilder('state, 'action, 'store) => {
  ...builder,
  queries: Some({applyQueryResult: applyQueryResult}),
};

let withLocalPersistence = (
  ~storeName: string,
  ~scopeKeyOfState: 'state => string,
  ~timestampOfState: 'state => float,
  ~stateElementId: option(string)=None,
  _,
  builder: queriesBuilder('state, 'action, 'store),
): local_input('state, 'action, 'store) => {
  schema: builder.schema,
  json: builder.json,
  guardTree: builder.guardTree,
  queries: builder.queries,
  persistence: {
    storeName,
    scopeKeyOfState,
    timestampOfState,
    stateElementId,
  },
};

let withSync = (
  ~transport: Sync.transportConfig('state, 'subscription),
  ~decodePatch: StoreJson.json => option('patch),
  ~updateOfPatch: ('patch, 'state) => 'state,
  ~setTimestamp: (~state: 'state, ~timestamp: float) => 'state,
  ~storeName: string,
  ~scopeKeyOfState: 'state => string,
  ~timestampOfState: 'state => float,
  ~streams: option(StoreRuntimeTypes.streamsConfig('patch, 'stream_event, 'streaming_state))=None,
  ~hooks: option(Sync.hooks('action))=?,
  ~stateElementId: option(string)=None,
  _,
  builder: queriesBuilder('state, 'action, 'store),
): synced_input('state, 'action, 'store, 'subscription, 'patch, 'stream_event, 'streaming_state) => {
  schema: builder.schema,
  json: builder.json,
  guardTree: builder.guardTree,
  queries: builder.queries,
  persistence: {
    storeName,
    scopeKeyOfState,
    timestampOfState,
    stateElementId,
    setTimestamp,
    transport,
  },
  hooks:
    switch (hooks) {
    | Some(h) => h
    | None => Sync.defaultHooks()
    },
  strategy: {decodePatch, updateOfPatch},
  streams,
};

let withSyncCrud = (
  ~transport: Sync.transportConfig('state, 'subscription),
  ~setTimestamp: (~state: 'state, ~timestamp: float) => 'state,
  ~storeName: string,
  ~scopeKeyOfState: 'state => string,
  ~timestampOfState: 'state => float,
  ~table: string,
  ~decodeRow: StoreJson.json => 'row,
  ~getId: 'row => string,
  ~getItems: 'state => array('row),
  ~setItems: ('state, array('row)) => 'state,
  ~hooks: option(Sync.hooks('action))=?,
  ~stateElementId: option(string)=None,
  _,
  builder: queriesBuilder('state, 'action, 'store),
): synced_crud_input('state, 'action, 'store, 'subscription, 'row) => {
  schema: builder.schema,
  json: builder.json,
  guardTree: builder.guardTree,
  queries: builder.queries,
  persistence: {
    storeName,
    scopeKeyOfState,
    timestampOfState,
    stateElementId,
    setTimestamp,
    transport,
  },
  hooks:
    switch (hooks) {
    | Some(h) => h
    | None => Sync.defaultHooks()
    },
  crud: {
    table,
    decodeRow,
    getId,
    getItems,
    setItems,
  },
};

/* ============================================================================
   Runtime
   ============================================================================ */

module Runtime = {
  type connection_status = StoreRuntimeTypes.connection_status =
    | NotApplicable
    | WaitingForOpen
    | Open;

  type status = StoreRuntimeTypes.status;

  module type Exports = {
    type state;
    type action;
    type t;
    type stream_event;
    type streaming_state;
    type listener_id = StoreEvents.listener_id;
    type store_event = StoreEvents.store_event(action);
    type listener = StoreEvents.listener(action);

    let empty: t;
    let hydrateStore: unit => t;
    let createStore: state => t;
    let serializeState: state => string;
    let serializeSnapshot: state => string;
    let dispatch: action => unit;
    let streaming: streaming_state;
    let flushCache: unit => Js.Promise.t(unit);
    let whenReady: unit => Js.Promise.t(unit);
    let whenIdle: unit => Js.Promise.t(unit);
    let status: unit => status;

    type status_listener_id = string;
    let subscribeStatus: (status => unit) => status_listener_id;
    let unsubscribeStatus: status_listener_id => unit;

    module Events: {
      let listen: listener => listener_id;
      let unlisten: listener_id => unit;
    };
  };

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
    let validate: option((~state: state, ~action: action) => StoreRuntimeTypes.guardResult);
    let queries: option(queriesConfig(state));
    let cache: [ | `IndexedDB | `None ];
  };

  module Make = StoreOffline.Local.Make;

  module type SyncedSchema = {
    type state;
    type action;
    type store;
    type subscription;
    type patch;
    type stream_event;
    type streaming_state;

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
    let streams: option(StoreRuntimeTypes.streamsConfig(patch, stream_event, streaming_state));
    let onActionError: string => unit;
    let onActionAck: option((~dispatch: action => unit, ~action: action, ~actionId: string) => unit);
    let onCustom: option(StoreJson.json => unit);
    let onMedia: option(StoreJson.json => unit);
    let onError: option((~dispatch: action => unit) => string => unit);
    let onOpen: option((~dispatch: action => unit) => unit);
    let onMultiplexedHandle: option(RealtimeClientMultiplexed.Multiplexed.t => unit);
    let validate: option((~state: state, ~action: action) => StoreRuntimeTypes.guardResult);
    let queries: option(queriesConfig(state));
    let cache: [ | `IndexedDB | `None ];
  };

  module MakeSynced = StoreOffline.Synced.Make;

  module type LocalStore = {
    include Exports;
    module Context: {
      let context: React.Context.t(t);
      let useStore: unit => t;
      module Provider: {
        type props = Js.t({. value: t, children: React.element});
        let makeProps: (~value: t, ~children: React.element, unit) => props;
        let make: props => React.element;
      };
    };
  };

  module type SyncedStore = {
    include Exports;
    module Context: {
      let context: React.Context.t(t);
      let useStore: unit => t;
      module Provider: {
        type props = Js.t({. value: t, children: React.element});
        let makeProps: (~value: t, ~children: React.element, unit) => props;
        let make: props => React.element;
      };
    };
  };
};

/* ============================================================================
   Terminal Builder Functions
   ============================================================================ */

let buildLocal =
    (type s, type a, type st, input: local_input(s, a, st))
    : (module Runtime.LocalStore with type state = s and type action = a and type t = st and type stream_event = unit and type streaming_state = unit) => {
  let stateElementId =
    switch (input.persistence.stateElementId) {
    | Some(value) => value
    | None => "initial-store"
    };

  module M =
    StoreOffline.Local.Make({
      type state = s;
      type action = a;
      type store = st;

      let reduce = input.schema.reduce;
      let emptyState = input.schema.emptyState;
      let storeName = input.persistence.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = input.persistence.scopeKeyOfState;
      let timestampOfState = input.persistence.timestampOfState;
      let state_of_json = input.json.state_of_json;
      let state_to_json = input.json.state_to_json;
      let action_of_json = input.json.action_of_json;
      let action_to_json = input.json.action_to_json;
      let makeStore = input.schema.makeStore;
      let validate =
        switch (input.guardTree) {
        | Some(tree) =>
          Some((~state, ~action) => GuardTree.resolve(~state, ~action, tree))
        | None => None
        };
      let queries = input.queries;
      let cache = `IndexedDB;
    });

  (module M: Runtime.LocalStore with type state = s and type action = a and type t = st and type stream_event = unit and type streaming_state = unit);
};

let buildSynced =
    (
      type s,
      type a,
      type st,
      type sub,
      type p,
      type se,
      type ss,
      input: synced_input(s, a, st, sub, p, se, ss),
    )
    : (module Runtime.SyncedStore with type state = s and type action = a and type t = st and type stream_event = se and type streaming_state = ss) => {
  let stateElementId =
    switch (input.persistence.stateElementId) {
    | Some(value) => value
    | None => "initial-store"
    };
  let hooks = input.hooks;

  module M =
    StoreOffline.Synced.Make({
      type state = s;
      type action = a;
      type store = st;
      type subscription = sub;
      type patch = p;
      type stream_event = se;
      type streaming_state = ss;

      let reduce = input.schema.reduce;
      let emptyState = input.schema.emptyState;
      let storeName = input.persistence.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = input.persistence.scopeKeyOfState;
      let timestampOfState = input.persistence.timestampOfState;
      let setTimestamp = input.persistence.setTimestamp;
      let state_of_json = input.json.state_of_json;
      let state_to_json = input.json.state_to_json;
      let action_of_json = input.json.action_of_json;
      let action_to_json = input.json.action_to_json;
      let makeStore = input.schema.makeStore;
      let subscriptionOfState = input.persistence.transport.subscriptionOfState;
      let encodeSubscription = input.persistence.transport.encodeSubscription;
      let eventUrl = input.persistence.transport.eventUrl;
      let baseUrl = input.persistence.transport.baseUrl;
      let decodePatch = input.strategy.decodePatch;
      let updateOfPatch = input.strategy.updateOfPatch;
      let streams = input.streams;
      let onActionError =
        switch (hooks.onActionError) {
        | Some(callback) => callback
        | None => Sync.defaultOnActionError
        };
      let onActionAck = hooks.onActionAck;
      let onCustom = hooks.onCustom;
      let onMedia = hooks.onMedia;
      let onError = hooks.onError;
      let onOpen = hooks.onOpen;
      let onMultiplexedHandle = hooks.onMultiplexedHandle;
      let validate =
        switch (input.guardTree) {
        | Some(tree) =>
          Some((~state, ~action) => GuardTree.resolve(~state, ~action, tree))
        | None => None
        };
      let queries = input.queries;
      let cache = `IndexedDB;
    });

  (module M: Runtime.SyncedStore with type state = s and type action = a and type t = st and type stream_event = se and type streaming_state = ss);
};

let buildCrud =
    (type s, type a, type st, type sub, type r, input: synced_crud_input(s, a, st, sub, r))
    : (module Runtime.SyncedStore with type state = s and type action = a and type t = st and type stream_event = unit and type streaming_state = unit) => {
  let stateElementId =
    switch (input.persistence.stateElementId) {
    | Some(value) => value
    | None => "initial-store"
    };
  let hooks = input.hooks;
  let crudPatch =
    StoreCrud.decodePatch(
      ~table=input.crud.table,
      ~decodeRow=input.crud.decodeRow,
      (),
    );
  let crudUpdate =
    StoreCrud.updateOfPatch(
      ~getId=input.crud.getId,
      ~getItems=input.crud.getItems,
      ~setItems=input.crud.setItems,
    );

  module M =
    StoreOffline.Synced.Make({
      type state = s;
      type action = a;
      type store = st;
      type subscription = sub;
      type patch = StoreCrud.patch(r);
      type stream_event = unit;
      type streaming_state = unit;

      let reduce = input.schema.reduce;
      let emptyState = input.schema.emptyState;
      let storeName = input.persistence.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = input.persistence.scopeKeyOfState;
      let timestampOfState = input.persistence.timestampOfState;
      let setTimestamp = input.persistence.setTimestamp;
      let state_of_json = input.json.state_of_json;
      let state_to_json = input.json.state_to_json;
      let action_of_json = input.json.action_of_json;
      let action_to_json = input.json.action_to_json;
      let makeStore = input.schema.makeStore;
      let subscriptionOfState = input.persistence.transport.subscriptionOfState;
      let encodeSubscription = input.persistence.transport.encodeSubscription;
      let eventUrl = input.persistence.transport.eventUrl;
      let baseUrl = input.persistence.transport.baseUrl;
      let decodePatch = StorePatch.compose([crudPatch]);
      let updateOfPatch = (patch, state) => crudUpdate(patch)(state);
      let streams = None;
      let onActionError =
        switch (hooks.onActionError) {
        | Some(callback) => callback
        | None => Sync.defaultOnActionError
        };
      let onActionAck = hooks.onActionAck;
      let onCustom = hooks.onCustom;
      let onMedia = hooks.onMedia;
      let onError = hooks.onError;
      let onOpen = hooks.onOpen;
      let onMultiplexedHandle = hooks.onMultiplexedHandle;
      let validate =
        switch (input.guardTree) {
        | Some(tree) =>
          Some((~state, ~action) => GuardTree.resolve(~state, ~action, tree))
        | None => None
        };
      let queries = input.queries;
      let cache = `IndexedDB;
    });

  (module M: Runtime.SyncedStore with type state = s and type action = a and type t = st and type stream_event = unit and type streaming_state = unit);
};

/* ==========================================================================
   CRUD Convenience Helpers
   ========================================================================== */

module Crud = {
  let totalCount =
      (~derive: option(Tilia.Core.deriver('store))=?, ~getItems: 'store => array('row), ()) =>
    Selectors.arrayLength(~derive?, ~getArray=getItems, ());

  let filteredCount =
      (
        ~derive: option(Tilia.Core.deriver('store))=?,
        ~getItems: 'store => array('row),
        ~predicate: 'row => bool,
        (),
      ) =>
    Selectors.filteredCount(~derive?, ~getArray=getItems, ~predicate, ());
};
