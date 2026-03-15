module type Schema = {
  type config;
  type payload;
  type projections;
  type store;

  let emptyStore: store;
  let payloadOfConfig: config => payload;
  let configOfPayload: payload => config;
  let project: config => projections;
  let makeStore: (~config: config, ~payload: payload) => store;
};

module Make = (Schema: Schema) => {
  type config = Schema.config;
  type payload = Schema.payload;
  type projections = Schema.projections;
  type t = Schema.store;

  let buildStore = (~configTransform=((config: config) => config), payload: payload) => {
    let config = payload->Schema.configOfPayload->configTransform;
    let _ = Schema.project;
    Schema.makeStore(~config, ~payload);
  };

  let empty = Schema.emptyStore;

  let createStore = (~configTransform=((config: config) => config), config: config) =>
    buildStore(~configTransform, config->Schema.payloadOfConfig);

  module Context = {
    let context = React.createContext(Schema.emptyStore);

    module Provider = {
      let make = React.Context.provider(context);
    };

    let useStore = () => React.useContext(context);
  };
};
