module type Schema = {
  type config;
  type patch;
  type payload;
  type projections;
  type store;

  let emptyStore: store;
  let stateElementId: string;

  let payloadOfConfig: config => payload;
  let configOfPayload: payload => config;
  let project: config => projections;
  let makeStore: (~config: config, ~payload: payload, ~projections: projections) => store;

  let encodeState: payload => string;
  let decodeState: Js.Json.t => payload;

  let channelOfConfig: config => option(string);
  let updatedAtOf: config => float;
  let decodeSnapshot: string => config;
  let parsePatch: Js.Json.t => option(patch);
  let applyPatch: (config, patch) => config;
  let eventUrl: string;
  let baseUrl: string;
};

module Make = (Schema: Schema) => {
  module Sync = StoreSync.Make({
    type config = Schema.config;
    type patch = Schema.patch;

    let channelOfConfig = Schema.channelOfConfig;
    let updatedAtOf = Schema.updatedAtOf;
    let decodeSnapshot = Schema.decodeSnapshot;
    let parsePatch = Schema.parsePatch;
    let applyPatch = Schema.applyPatch;
    let eventUrl = Schema.eventUrl;
    let baseUrl = Schema.baseUrl;
  });

  let buildStore = (payload: Schema.payload) => {
    let config = payload->Schema.configOfPayload;
    let syncedConfig = Sync.source(config);
    let projections =
      switch%platform (Runtime.platform) {
      | Client => Tilia.Core.computed(() => Schema.project(syncedConfig))
      | Server => Schema.project(syncedConfig)
      };
    Schema.makeStore(~config=syncedConfig, ~payload, ~projections);
  };

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

  let empty = Schema.emptyStore;
  let createStore = (config: config) => config->Schema.payloadOfConfig->buildStore;
  let hydrateStore = State.hydrateStore;
  let serializeState = (config: config) =>
    config->Schema.payloadOfConfig->State.serializeState;

  module Context = {
    let context = React.createContext(Schema.emptyStore);

    module Provider = {
      let make = React.Context.provider(context);
    };

    let useStore = () => React.useContext(context);
  };
};
