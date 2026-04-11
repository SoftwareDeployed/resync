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

  /* Hydrate a store and wrap children with its provider.
     Returns a record with both the store (for prop passing) and the wrapped element.

     Usage:
       let result = Bootstrap.withHydratedProvider(
         ~hydrateStore=TodoStore.hydrateStore,
         ~provider=TodoStore.Context.Provider.make,
         ~children=<App />,
       );
       ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
  */
  let withHydratedProvider = (~hydrateStore, ~provider, ~children) => {
    let store = hydrateStore();
    let element =
      React.createElement(
        provider,
        {"value": store, "children": children},
      );
    {store, element};
  };

  /* Hydrate multiple stores and wrap children with nested providers.
     Stores are hydrated in order; first store is outermost provider.
     The store list is returned alongside the wrapped element for prop passing.

     Usage:
       let result = Bootstrap.withHydratedProviders(
         ~stores=[|
           ("store", Store.hydrateStore, Store.Context.Provider.make),
           ("cart", CartStore.hydrateStore, CartStore.Context.Provider.make),
         |],
         ~children=<App />,
       );
       ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
  */
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

  /* Create a store on the server and wrap children with its provider.
     For SSR entrypoints that use createStore instead of hydrateStore.
     Returns a record with both the store and the wrapped element.

     Usage:
       let result = Bootstrap.withCreatedProvider(
         ~createStore=TodoStore.createStore,
         ~provider=TodoStore.Context.Provider.make,
         ~initialState=preloadedState,
         ~children=<App />,
       );
  */
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

  /* Stub implementations for native - these shouldn't be called on server */
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
    /* Legacy compatibility hooks. New code should prefer Runtime.Events.listen and
       the narrow StoreEvents.store_event surface instead of raw per-frame callback
       registration. */
    let onActionError: string => unit;
    let onActionAck: option((~dispatch: action => unit, ~action: action, ~actionId: string) => unit);
    let onCustom: option(StoreJson.json => unit);
    let onMedia: option(StoreJson.json => unit);
    let onError: option((~dispatch: action => unit) => string => unit);
    let onOpen: option((~dispatch: action => unit) => unit);
    /* Optional hook called when a connection handle is created. */
    let onConnectionHandle: option(RealtimeClient.Socket.connection_handle => unit);
    let cache: [ | `IndexedDB | `None ];
  };

  module MakeSynced = StoreOffline.Synced.Make;
};

module Local = {
  type config('state, 'action, 'store) = {
    storeName: string,
    emptyState: 'state,
    reduce: (~state: 'state, ~action: 'action) => 'state,
    state_of_json: StoreJson.json => 'state,
    state_to_json: 'state => StoreJson.json,
    action_of_json: StoreJson.json => 'action,
    action_to_json: 'action => StoreJson.json,
    makeStore: (~state: 'state, ~derive: Tilia.Core.deriver('store)=?, unit) => 'store,
    scopeKeyOfState: 'state => string,
    timestampOfState: 'state => float,
    stateElementId: option(string),
  };

  module type Input = {
    type state;
    type action;
    type store;
    let config: config(state, action, store);
  };

  module Define = (Input: Input) => {
    let stateElementId =
      switch (Input.config.stateElementId) {
      | Some(value) => value
      | None => "initial-store"
      };

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;

      let reduce = Input.config.reduce;
      let emptyState = Input.config.emptyState;
      let storeName = Input.config.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.config.scopeKeyOfState;
      let timestampOfState = Input.config.timestampOfState;
      let state_of_json = Input.config.state_of_json;
      let state_to_json = Input.config.state_to_json;
      let action_of_json = Input.config.action_of_json;
      let action_to_json = Input.config.action_to_json;
      let makeStore = Input.config.makeStore;
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

  let defaultHooks = (): hooks('action) => {
    onActionError: None,
    onActionAck: None,
    onCustom: None,
    onMedia: None,
    onError: None,
    onOpen: None,
    onConnectionHandle: None,
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

module Synced = {
  type baseConfig('state, 'action, 'store, 'subscription) = {
    storeName: string,
    emptyState: 'state,
    reduce: (~state: 'state, ~action: 'action) => 'state,
    state_of_json: StoreJson.json => 'state,
    state_to_json: 'state => StoreJson.json,
    action_of_json: StoreJson.json => 'action,
    action_to_json: 'action => StoreJson.json,
    makeStore: (~state: 'state, ~derive: Tilia.Core.deriver('store)=?, unit) => 'store,
    scopeKeyOfState: 'state => string,
    timestampOfState: 'state => float,
    setTimestamp: (~state: 'state, ~timestamp: float) => 'state,
    transport: Sync.transportConfig('state, 'subscription),
    stateElementId: option(string),
    hooks: option(Sync.hooks('action)),
  };

  module type Input = {
    type state;
    type action;
    type store;
    type subscription;
    type patch;
    type stream_event;
    type streaming_state;
    let base: baseConfig(state, action, store, subscription);
    let strategy: Sync.customStrategy(state, patch);
    let streams: option(StoreRuntimeTypes.streamsConfig(patch, stream_event, streaming_state));
  };

  module Define = (Input: Input) => {
    let stateElementId =
      switch (Input.base.stateElementId) {
      | Some(value) => value
      | None => "initial-store"
      };
    let hooks =
      switch (Input.base.hooks) {
      | Some(value) => value
      | None => Sync.defaultHooks()
      };

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;
      type subscription = Input.subscription;
      type patch = Input.patch;
      type stream_event = Input.stream_event;
      type streaming_state = Input.streaming_state;

      let reduce = Input.base.reduce;
      let emptyState = Input.base.emptyState;
      let storeName = Input.base.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.base.scopeKeyOfState;
      let timestampOfState = Input.base.timestampOfState;
      let setTimestamp = Input.base.setTimestamp;
      let state_of_json = Input.base.state_of_json;
      let state_to_json = Input.base.state_to_json;
      let action_of_json = Input.base.action_of_json;
      let action_to_json = Input.base.action_to_json;
      let makeStore = Input.base.makeStore;
      let subscriptionOfState = Input.base.transport.subscriptionOfState;
      let encodeSubscription = Input.base.transport.encodeSubscription;
      let eventUrl = Input.base.transport.eventUrl;
      let baseUrl = Input.base.transport.baseUrl;
      let decodePatch = Input.strategy.decodePatch;
      let updateOfPatch = Input.strategy.updateOfPatch;
      let streams = Input.streams;
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
    let base: baseConfig(state, action, store, subscription);
    let strategy: Sync.crudStrategy(state, row);
  };

  module DefineCrud = (Input: CrudInput) => {
    let stateElementId =
      switch (Input.base.stateElementId) {
      | Some(value) => value
      | None => "initial-store"
      };
    let hooks =
      switch (Input.base.hooks) {
      | Some(value) => value
      | None => Sync.defaultHooks()
      };
    let crudPatch =
      StoreCrud.decodePatch(
        ~table=Input.strategy.crud.table,
        ~decodeRow=Input.strategy.crud.decodeRow,
        (),
      );
    let crudUpdate =
      StoreCrud.updateOfPatch(
        ~getId=Input.strategy.crud.getId,
        ~getItems=Input.strategy.crud.getItems,
        ~setItems=Input.strategy.crud.setItems,
      );

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;
      type subscription = Input.subscription;
      type patch = StoreCrud.patch(Input.row);
      type stream_event = unit;
      type streaming_state = unit;

      let reduce = Input.base.reduce;
      let emptyState = Input.base.emptyState;
      let storeName = Input.base.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.base.scopeKeyOfState;
      let timestampOfState = Input.base.timestampOfState;
      let setTimestamp = Input.base.setTimestamp;
      let state_of_json = Input.base.state_of_json;
      let state_to_json = Input.base.state_to_json;
      let action_of_json = Input.base.action_of_json;
      let action_to_json = Input.base.action_to_json;
      let makeStore = Input.base.makeStore;
      let subscriptionOfState = Input.base.transport.subscriptionOfState;
      let encodeSubscription = Input.base.transport.encodeSubscription;
      let eventUrl = Input.base.transport.eventUrl;
      let baseUrl = Input.base.transport.baseUrl;
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
     
     These helpers further reduce boilerplate for standard CRUD stores by
     pre-wiring common patterns like item counts and field projections.
     They layer on top of DefineCrud (Task 4 API) and remain opt-in.
     ========================================================================== */

  module Crud = {
    /* Total count derived selector for CRUD items.
       Usage in makeStore:
         total_count: Crud.totalCount(~derive?, ~getItems=state => state.items, ()),
    */
    let totalCount =
        (~derive: option(Tilia.Core.deriver('store))=?, ~getItems: 'store => array('row), ()) =>
      Selectors.arrayLength(~derive?, ~getArray=getItems, ());

    /* Filtered count derived selector for CRUD items.
       Usage in makeStore:
         completed_count: Crud.filteredCount(~derive?, ~getItems=store => store.state.items, ~predicate=item => item.completed, ()),
    */
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
