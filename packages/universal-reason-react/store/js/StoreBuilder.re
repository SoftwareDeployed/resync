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

module Runtime = {
  module type Exports = {
    type config;
    type payload;
    type t;

    let empty: t;
    let createStore: config => t;
    let hydrateStore: unit => t;
    let serializeState: config => string;
    let serializeSnapshot: config => string;
  };

  module type Schema = {
    type config;
    type patch;
    type payload;
    type store;
    type subscription;

    let emptyStore: store;
    let stateElementId: string;

    let payloadOfConfig: config => payload;
    let configOfPayload: payload => config;

    let makeStore:
      (~config: config, ~payload: payload, ~derive: Tilia.Core.deriver(store)=?, unit) =>
      store;

    let config_of_json: StoreJson.json => config;
    let config_to_json: config => StoreJson.json;
    let payload_of_json: StoreJson.json => payload;
    let payload_to_json: payload => StoreJson.json;
    let decodePatch: StoreJson.json => option(patch);

    let subscriptionOfConfig: config => option(subscription);
    let encodeSubscription: subscription => string;
    let updatedAtOf: config => float;
    let updateOfPatch: patch => config => config;
    let eventUrl: string;
    let baseUrl: string;
  };

  module Make = (Schema: Schema) =>
    StoreRuntime.Make({
      type config = Schema.config;
      type patch = Schema.patch;
      type payload = Schema.payload;
      type projections = unit;
      type store = Schema.store;
      type subscription = Schema.subscription;

      let emptyStore = Schema.emptyStore;
      let stateElementId = Schema.stateElementId;
      let payloadOfConfig = Schema.payloadOfConfig;
      let configOfPayload = Schema.configOfPayload;
      let project = (_config: config) => ();
      let makeStore = (~config, ~payload) =>
        StoreComputed.make(
          ~client=derive => Schema.makeStore(~config, ~payload, ~derive, ()),
          ~server=() => Schema.makeStore(~config, ~payload, ()),
        );
      let config_of_json = Schema.config_of_json;
      let config_to_json = Schema.config_to_json;
      let payload_of_json = Schema.payload_of_json;
      let payload_to_json = Schema.payload_to_json;
      let decodePatch = Schema.decodePatch;
      let subscriptionOfConfig = Schema.subscriptionOfConfig;
      let encodeSubscription = Schema.encodeSubscription;
      let updatedAtOf = Schema.updatedAtOf;
      let updateOfPatch = Schema.updateOfPatch;
      let eventUrl = Schema.eventUrl;
      let baseUrl = Schema.baseUrl;
    });
};

module Persisted = {
  module type Exports = {
    type config;
    type payload;
    type t;

    let empty: t;
    let createStore: config => t;
    let hydrateStore: unit => t;
    let serializeState: config => string;
    let persistPayload: payload => unit;
    let persistStore: t => unit;
    let clear: unit => unit;
  };

  module type Schema = {
    type config;
    type payload;
    type store;

    let storageKey: string;
    let emptyStore: store;
    let payloadOfConfig: config => payload;
    let configOfPayload: payload => config;
    let payloadOfStore: store => payload;
    let payload_of_json: StoreJson.json => payload;
    let payload_to_json: payload => StoreJson.json;
    let transformConfig: config => config;

    let makeStore:
      (~config: config, ~payload: payload, ~derive: Tilia.Core.deriver(store)=?, unit) =>
      store;
  };

  module Make = (Schema: Schema) => {
    module Core = StoreCore.Make({
      type config = Schema.config;
      type payload = Schema.payload;
      type projections = unit;
      type store = Schema.store;

      let emptyStore = Schema.emptyStore;
      let payloadOfConfig = Schema.payloadOfConfig;
      let configOfPayload = Schema.configOfPayload;
      let project = (_config: config) => ();
      let makeStore = (~config, ~payload) =>
        StoreComputed.make(
          ~client=derive => Schema.makeStore(~config, ~payload, ~derive, ()),
          ~server=() => Schema.makeStore(~config, ~payload, ()),
        );
    });

    module Persist = StorePersist.Make({
      type payload = Schema.payload;
      type store = Schema.store;

      let storageKey = Schema.storageKey;
      let emptyStore = Schema.emptyStore;
      let makeStore = payload => Core.buildStore(~configTransform=Schema.transformConfig, payload);
      let payloadOfStore = Schema.payloadOfStore;
      let payload_of_json = Schema.payload_of_json;
      let payload_to_json = Schema.payload_to_json;
    });

    type config = Schema.config;
    type payload = Schema.payload;
    type t = Schema.store;

    let empty = Core.empty;
    let createStore = (config: config) => Core.createStore(~configTransform=Schema.transformConfig, config);
    let hydrateStore = Persist.hydrateStore;
    let serializeState = (config: config) =>
      StoreJson.stringify(Schema.payload_to_json, config->Schema.payloadOfConfig);
    let persistPayload = Persist.persistPayload;
    let persistStore = Persist.persistStore;
    let clear = Persist.clear;

    module Context = Core.Context;
  };
};
