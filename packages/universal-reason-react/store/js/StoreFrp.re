module Local = {
  type schema('state, 'action, 'store) = {
    storeName: string,
    emptyState: 'state,
    reduce: (~state: 'state, ~action: 'action) => 'state,
    state_of_json: Store.Json.json => 'state,
    state_to_json: 'state => Store.Json.json,
    action_of_json: Store.Json.json => 'action,
    action_to_json: 'action => Store.Json.json,
    makeStore: (~state: 'state, ~derive: Tilia.Core.deriver('store)=?, unit) => 'store,
    scopeKeyOfState: 'state => string,
    timestampOfState: 'state => float,
    stateElementId: option(string),
  };

  type config('state, 'action, 'store) = {
    schema: schema('state, 'action, 'store),
    cache: [ | `IndexedDB | `None ],
  };

  let make = (schema: schema('state, 'action, 'store)): config('state, 'action, 'store) => {
    schema,
    cache: `None,
  };

  let withCache = (cache: [ | `IndexedDB | `None ], config: config('state, 'action, 'store)): config('state, 'action, 'store) => {
    ...config,
    cache,
  };

  module type BuildInput = {
    type state;
    type action;
    type store;
    let config: config(state, action, store);
  };

  module Build = (Input: BuildInput) => {
    let stateElementId =
      switch (Input.config.schema.stateElementId) {
      | Some(id) => id
      | None => "initial-store"
      };

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;

      let reduce = Input.config.schema.reduce;
      let emptyState = Input.config.schema.emptyState;
      let storeName = Input.config.schema.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.config.schema.scopeKeyOfState;
      let timestampOfState = Input.config.schema.timestampOfState;
      let state_of_json = Input.config.schema.state_of_json;
      let state_to_json = Input.config.schema.state_to_json;
      let action_of_json = Input.config.schema.action_of_json;
      let action_to_json = Input.config.schema.action_to_json;
      let makeStore = Input.config.schema.makeStore;
      let cache = Input.config.cache;
    };

    include StoreOffline.Local.Make(Schema);
  };
};

module Synced = {
  type schema('state, 'action, 'store) = {
    storeName: string,
    emptyState: 'state,
    reduce: (~state: 'state, ~action: 'action) => 'state,
    state_of_json: Store.Json.json => 'state,
    state_to_json: 'state => Store.Json.json,
    action_of_json: Store.Json.json => 'action,
    action_to_json: 'action => Store.Json.json,
    makeStore: (~state: 'state, ~derive: Tilia.Core.deriver('store)=?, unit) => 'store,
    scopeKeyOfState: 'state => string,
    timestampOfState: 'state => float,
    setTimestamp: (~state: 'state, ~timestamp: float) => 'state,
    stateElementId: option(string),
  };

  type config('state, 'action, 'store, 'sub, 'patch) = {
    schema: schema('state, 'action, 'store),
    transport: StoreBuilder.Sync.transportConfig('state, 'sub),
    strategy: StoreBuilder.Sync.customStrategy('state, 'patch),
    hooks: option(StoreBuilder.Sync.hooks('action)),
    cache: [ | `IndexedDB | `None ],
  };

  let make = (~transport, ~strategy, ~hooks=?, schema): config('state, 'action, 'store, 'sub, 'patch) => {
    schema,
    transport,
    strategy,
    hooks,
    cache: `None,
  };

  let withCache = (cache, config) => {
    ...config,
    cache,
  };

  module type BuildInput = {
    type state;
    type action;
    type store;
    type subscription;
    type patch;
    let config: config(state, action, store, subscription, patch);
  };

  module Build = (Input: BuildInput) => {
    let stateElementId =
      switch (Input.config.schema.stateElementId) {
      | Some(id) => id
      | None => "initial-store"
      };

    let hooks =
      switch (Input.config.hooks) {
      | Some(h) => h
      | None => StoreBuilder.Sync.defaultHooks()
      };

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;
      type subscription = Input.subscription;
      type patch = Input.patch;
      type stream_event = unit;
      type streaming_state = unit;

      let reduce = Input.config.schema.reduce;
      let emptyState = Input.config.schema.emptyState;
      let storeName = Input.config.schema.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.config.schema.scopeKeyOfState;
      let timestampOfState = Input.config.schema.timestampOfState;
      let setTimestamp = Input.config.schema.setTimestamp;
      let state_of_json = Input.config.schema.state_of_json;
      let state_to_json = Input.config.schema.state_to_json;
      let action_of_json = Input.config.schema.action_of_json;
      let action_to_json = Input.config.schema.action_to_json;
      let makeStore = Input.config.schema.makeStore;
      let subscriptionOfState = Input.config.transport.subscriptionOfState;
      let encodeSubscription = Input.config.transport.encodeSubscription;
      let eventUrl = Input.config.transport.eventUrl;
      let baseUrl = Input.config.transport.baseUrl;
      let decodePatch = Input.config.strategy.decodePatch;
      let updateOfPatch = Input.config.strategy.updateOfPatch;
      let streams = None;
      let onActionError =
        switch (hooks.onActionError) {
        | Some(cb) => cb
        | None => StoreBuilder.Sync.defaultOnActionError
        };
      let onActionAck = hooks.onActionAck;
      let onCustom = hooks.onCustom;
      let onMedia = hooks.onMedia;
      let onError = hooks.onError;
      let onOpen = hooks.onOpen;
      let onConnectionHandle = hooks.onConnectionHandle;
      let cache = Input.config.cache;
    };

    include StoreOffline.Synced.Make(Schema);
  };
};

module Crud = {
  type schema('state, 'action, 'store) = {
    storeName: string,
    emptyState: 'state,
    reduce: (~state: 'state, ~action: 'action) => 'state,
    state_of_json: Store.Json.json => 'state,
    state_to_json: 'state => Store.Json.json,
    action_of_json: Store.Json.json => 'action,
    action_to_json: 'action => Store.Json.json,
    makeStore: (~state: 'state, ~derive: Tilia.Core.deriver('store)=?, unit) => 'store,
    scopeKeyOfState: 'state => string,
    timestampOfState: 'state => float,
    setTimestamp: (~state: 'state, ~timestamp: float) => 'state,
    stateElementId: option(string),
  };

  type config('state, 'action, 'store, 'sub, 'row) = {
    schema: schema('state, 'action, 'store),
    transport: StoreBuilder.Sync.transportConfig('state, 'sub),
    strategy: StoreBuilder.Sync.crudStrategy('state, 'row),
    hooks: option(StoreBuilder.Sync.hooks('action)),
    cache: [ | `IndexedDB | `None ],
  };

  let make = (~transport, ~strategy, ~hooks=?, schema): config('state, 'action, 'store, 'sub, 'row) => {
    schema,
    transport,
    strategy,
    hooks,
    cache: `None,
  };

  let withCache = (cache, config) => {
    ...config,
    cache,
  };

  module type BuildInput = {
    type state;
    type action;
    type store;
    type subscription;
    type row;
    let config: config(state, action, store, subscription, row);
  };

  module Build = (Input: BuildInput) => {
    let stateElementId =
      switch (Input.config.schema.stateElementId) {
      | Some(id) => id
      | None => "initial-store"
      };

    let hooks =
      switch (Input.config.hooks) {
      | Some(h) => h
      | None => StoreBuilder.Sync.defaultHooks()
      };

    let crudPatch =
      Store.Crud.decodePatch(
        ~table=Input.config.strategy.crud.table,
        ~decodeRow=Input.config.strategy.crud.decodeRow,
        (),
      );

    let crudUpdate =
      Store.Crud.updateOfPatch(
        ~getId=Input.config.strategy.crud.getId,
        ~getItems=Input.config.strategy.crud.getItems,
        ~setItems=Input.config.strategy.crud.setItems,
      );

    module Schema = {
      type state = Input.state;
      type action = Input.action;
      type store = Input.store;
      type subscription = Input.subscription;
      type patch = Store.Crud.patch(Input.row);
      type stream_event = unit;
      type streaming_state = unit;

      let reduce = Input.config.schema.reduce;
      let emptyState = Input.config.schema.emptyState;
      let storeName = Input.config.schema.storeName;
      let stateElementId = stateElementId;
      let scopeKeyOfState = Input.config.schema.scopeKeyOfState;
      let timestampOfState = Input.config.schema.timestampOfState;
      let setTimestamp = Input.config.schema.setTimestamp;
      let state_of_json = Input.config.schema.state_of_json;
      let state_to_json = Input.config.schema.state_to_json;
      let action_of_json = Input.config.schema.action_of_json;
      let action_to_json = Input.config.schema.action_to_json;
      let makeStore = Input.config.schema.makeStore;
      let subscriptionOfState = Input.config.transport.subscriptionOfState;
      let encodeSubscription = Input.config.transport.encodeSubscription;
      let eventUrl = Input.config.transport.eventUrl;
      let baseUrl = Input.config.transport.baseUrl;
      let decodePatch = Store.Patch.compose([crudPatch]);
      let updateOfPatch = (patch, state) => crudUpdate(patch)(state);
      let streams = None;
      let onActionError =
        switch (hooks.onActionError) {
        | Some(cb) => cb
        | None => StoreBuilder.Sync.defaultOnActionError
        };
      let onActionAck = hooks.onActionAck;
      let onCustom = hooks.onCustom;
      let onMedia = hooks.onMedia;
      let onError = hooks.onError;
      let onOpen = hooks.onOpen;
      let onConnectionHandle = hooks.onConnectionHandle;
      let cache = Input.config.cache;
    };

    include StoreOffline.Synced.Make(Schema);
  };
};
