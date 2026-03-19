module type Schema = {
  type config;
  type patch;
  type payload;
  type projections;
  type store;
  type subscription;

  let emptyStore: store;
  let stateElementId: string;

  let payloadOfConfig: config => payload;
  let configOfPayload: payload => config;
  let project: config => projections;
  let makeStore: (~config: config, ~payload: payload) => store;

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

module Make = (Schema: Schema) => {
  module Core = StoreCore.Make({
    type config = Schema.config;
    type payload = Schema.payload;
    type projections = Schema.projections;
    type store = Schema.store;

    let emptyStore = Schema.emptyStore;
    let payloadOfConfig = Schema.payloadOfConfig;
    let configOfPayload = Schema.configOfPayload;
    let project = Schema.project;
    let makeStore = Schema.makeStore;
  });

  module Sync = StoreSync.Make({
    type config = Schema.config;
    type patch = Schema.patch;
    type subscription = Schema.subscription;

      let subscriptionOfConfig = Schema.subscriptionOfConfig;
      let encodeSubscription = Schema.encodeSubscription;
      let updatedAtOf = Schema.updatedAtOf;
      let config_of_json = Schema.config_of_json;
      let decodePatch = Schema.decodePatch;
      let updateOfPatch = Schema.updateOfPatch;
      let eventUrl = Schema.eventUrl;
      let baseUrl = Schema.baseUrl;
    });

  let buildStore = (payload: Schema.payload) =>
    Core.buildStore(~configTransform=Sync.source, payload);

  module State = StoreState.Make({
    type payload = Schema.payload;
    type store = Schema.store;

    let emptyStore = Schema.emptyStore;
    let makeStore = buildStore;
    let payload_of_json = Schema.payload_of_json;
    let payload_to_json = Schema.payload_to_json;
    let stateElementId = Schema.stateElementId;
  });

  type config = Schema.config;
  type patch = Schema.patch;
  type payload = Schema.payload;
  type projections = Schema.projections;
  type t = Schema.store;

  let empty = Core.empty;
  let createStore = (config: config) => Core.createStore(~configTransform=Sync.source, config);
  let hydrateStore = State.hydrateStore;
  let serializeState = (config: config) =>
    config->Schema.payloadOfConfig->State.serializeState;
  let serializeSnapshot = (config: config) => StoreJson.stringify(Schema.config_to_json, config);

  module Context = Core.Context;
};
