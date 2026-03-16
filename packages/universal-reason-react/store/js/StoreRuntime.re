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

  let encodeState: payload => string;
  let decodeState: Js.Json.t => payload;

  let subscriptionOfConfig: config => option(subscription);
  let encodeSubscription: subscription => string;
  let updatedAtOf: config => float;
  let decodeSnapshot: string => config;
  let parsePatch: Js.Json.t => option(patch);
  let applyPatch: (config, patch) => config;
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
    let decodeSnapshot = Schema.decodeSnapshot;
    let parsePatch = Schema.parsePatch;
    let applyPatch = Schema.applyPatch;
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
    let decodeState = Schema.decodeState;
    let encodeState = Schema.encodeState;
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

  module Context = Core.Context;
};
