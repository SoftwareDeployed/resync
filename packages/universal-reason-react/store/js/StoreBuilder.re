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
    onConnectionHandle: option(RealtimeClient.Socket.connection_handle => unit),
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
    onConnectionHandle: None,
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
  persistence: localPersistence('state),
};

type synced_input('state, 'action, 'store, 'subscription, 'patch, 'stream_event, 'streaming_state) = {
  schema: schema('state, 'action, 'store),
  json: json('state, 'action),
  guardTree: option(GuardTree.t('state, 'action)),
  persistence: syncPersistence('state, 'subscription),
  hooks: Sync.hooks('action),
  strategy: Sync.customStrategy('state, 'patch),
  streams: option(StoreRuntimeTypes.streamsConfig('patch, 'stream_event, 'streaming_state)),
};

type synced_crud_input('state, 'action, 'store, 'subscription, 'row) = {
  schema: schema('state, 'action, 'store),
  json: json('state, 'action),
  guardTree: option(GuardTree.t('state, 'action)),
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
): jsonBuilder('state, 'action, 'store) => {
  schema: builder.schema,
  json: {state_of_json, state_to_json, action_of_json, action_to_json},
  guardTree: builder.guardTree,
};

let withLocalPersistence = (
  ~storeName: string,
  ~scopeKeyOfState: 'state => string,
  ~timestampOfState: 'state => float,
  ~stateElementId: option(string)=None,
  _,
  builder: jsonBuilder('state, 'action, 'store),
): local_input('state, 'action, 'store) => {
  schema: builder.schema,
  json: builder.json,
  guardTree: builder.guardTree,
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
  builder: jsonBuilder('state, 'action, 'store),
): synced_input('state, 'action, 'store, 'subscription, 'patch, 'stream_event, 'streaming_state) => {
  schema: builder.schema,
  json: builder.json,
  guardTree: builder.guardTree,
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
  builder: jsonBuilder('state, 'action, 'store),
): synced_crud_input('state, 'action, 'store, 'subscription, 'row) => {
  schema: builder.schema,
  json: builder.json,
  guardTree: builder.guardTree,
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
    let onConnectionHandle: option(RealtimeClient.Socket.connection_handle => unit);
    let validate: option((~state: state, ~action: action) => StoreRuntimeTypes.guardResult);
    let cache: [ | `IndexedDB | `None ];
  };

  module MakeSynced = StoreOffline.Synced.Make;
};

/* ============================================================================
   Pipeline Definition Functors
   ============================================================================ */

module Local = {
  module type Input = {
    type state;
    type action;
    type store;
    let input: local_input(state, action, store);
  };

  module Define = (Input: Input) => {
    let stateElementId =
      switch (Input.input.persistence.stateElementId) {
      | Some(value) => value
      | None => "initial-store"
      };

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;

      let reduce = Input.input.schema.reduce;
      let emptyState = Input.input.schema.emptyState;
      let storeName = Input.input.persistence.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.input.persistence.scopeKeyOfState;
      let timestampOfState = Input.input.persistence.timestampOfState;
      let state_of_json = Input.input.json.state_of_json;
      let state_to_json = Input.input.json.state_to_json;
      let action_of_json = Input.input.json.action_of_json;
      let action_to_json = Input.input.json.action_to_json;
      let makeStore = Input.input.schema.makeStore;
      let validate =
        switch (Input.input.guardTree) {
        | Some(tree) => Some((~state, ~action) => GuardTree.resolve(~state, ~action, tree))
        | None => None
        };
      let cache = `IndexedDB;
    };

    include StoreOffline.Local.Make(Schema);

    let originalContext = Context.context;
    let originalProviderMake = Context.Provider.make;
    let originalProviderMakeProps = Context.Provider.makeProps;
    let originalUseStore = Context.useStore;

    module Context = {
      let context = originalContext;
      let useStore = originalUseStore;

      module Provider = {
        let makeProps = originalProviderMakeProps;
        let make = originalProviderMake;
      };
    };
  };
};

module Synced = {
  module type Input = {
    type state;
    type action;
    type store;
    type subscription;
    type patch;
    type stream_event;
    type streaming_state;
    let input: synced_input(state, action, store, subscription, patch, stream_event, streaming_state);
  };

  module Define = (Input: Input) => {
    let stateElementId =
      switch (Input.input.persistence.stateElementId) {
      | Some(value) => value
      | None => "initial-store"
      };
    let hooks = Input.input.hooks;

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;
      type subscription = Input.subscription;
      type patch = Input.patch;
      type stream_event = Input.stream_event;
      type streaming_state = Input.streaming_state;

      let reduce = Input.input.schema.reduce;
      let emptyState = Input.input.schema.emptyState;
      let storeName = Input.input.persistence.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.input.persistence.scopeKeyOfState;
      let timestampOfState = Input.input.persistence.timestampOfState;
      let setTimestamp = Input.input.persistence.setTimestamp;
      let state_of_json = Input.input.json.state_of_json;
      let state_to_json = Input.input.json.state_to_json;
      let action_of_json = Input.input.json.action_of_json;
      let action_to_json = Input.input.json.action_to_json;
      let makeStore = Input.input.schema.makeStore;
      let subscriptionOfState = Input.input.persistence.transport.subscriptionOfState;
      let encodeSubscription = Input.input.persistence.transport.encodeSubscription;
      let eventUrl = Input.input.persistence.transport.eventUrl;
      let baseUrl = Input.input.persistence.transport.baseUrl;
      let decodePatch = Input.input.strategy.decodePatch;
      let updateOfPatch = Input.input.strategy.updateOfPatch;
      let streams = Input.input.streams;
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
      let onConnectionHandle = hooks.onConnectionHandle;
      let validate =
        switch (Input.input.guardTree) {
        | Some(tree) => Some((~state, ~action) => GuardTree.resolve(~state, ~action, tree))
        | None => None
        };
      let cache = `IndexedDB;
    };

    include StoreOffline.Synced.Make(Schema);

    let originalContext = Context.context;
    let originalProviderMake = Context.Provider.make;
    let originalProviderMakeProps = Context.Provider.makeProps;
    let originalUseStore = Context.useStore;

    module Context = {
      let context = originalContext;
      let useStore = originalUseStore;

      module Provider = {
        let makeProps = originalProviderMakeProps;
        let make = originalProviderMake;
      };
    };
  };

  module type CrudInput = {
    type state;
    type action;
    type store;
    type subscription;
    type row;
    let input: synced_crud_input(state, action, store, subscription, row);
  };

  module DefineCrud = (Input: CrudInput) => {
    let stateElementId =
      switch (Input.input.persistence.stateElementId) {
      | Some(value) => value
      | None => "initial-store"
      };
    let hooks = Input.input.hooks;
    let crudPatch =
      StoreCrud.decodePatch(
        ~table=Input.input.crud.table,
        ~decodeRow=Input.input.crud.decodeRow,
        (),
      );
    let crudUpdate =
      StoreCrud.updateOfPatch(
        ~getId=Input.input.crud.getId,
        ~getItems=Input.input.crud.getItems,
        ~setItems=Input.input.crud.setItems,
      );

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;
      type subscription = Input.subscription;
      type patch = StoreCrud.patch(Input.row);
      type stream_event = unit;
      type streaming_state = unit;

      let reduce = Input.input.schema.reduce;
      let emptyState = Input.input.schema.emptyState;
      let storeName = Input.input.persistence.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.input.persistence.scopeKeyOfState;
      let timestampOfState = Input.input.persistence.timestampOfState;
      let setTimestamp = Input.input.persistence.setTimestamp;
      let state_of_json = Input.input.json.state_of_json;
      let state_to_json = Input.input.json.state_to_json;
      let action_of_json = Input.input.json.action_of_json;
      let action_to_json = Input.input.json.action_to_json;
      let makeStore = Input.input.schema.makeStore;
      let subscriptionOfState = Input.input.persistence.transport.subscriptionOfState;
      let encodeSubscription = Input.input.persistence.transport.encodeSubscription;
      let eventUrl = Input.input.persistence.transport.eventUrl;
      let baseUrl = Input.input.persistence.transport.baseUrl;
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
      let onConnectionHandle = hooks.onConnectionHandle;
      let validate =
        switch (Input.input.guardTree) {
        | Some(tree) => Some((~state, ~action) => GuardTree.resolve(~state, ~action, tree))
        | None => None
        };
      let cache = `IndexedDB;
    };

    include StoreOffline.Synced.Make(Schema);

    let originalContext = Context.context;
    let originalProviderMake = Context.Provider.make;
    let originalProviderMakeProps = Context.Provider.makeProps;
    let originalUseStore = Context.useStore;

    module Context = {
      let context = originalContext;
      let useStore = originalUseStore;

      module Provider = {
        let makeProps = originalProviderMakeProps;
        let make = originalProviderMake;
      };
    };
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
