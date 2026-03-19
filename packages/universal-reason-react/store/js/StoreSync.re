module type Schema = {
  type config;
  type patch;
  type subscription;

  let subscriptionOfConfig: config => option(subscription);
  let encodeSubscription: subscription => string;
  let updatedAtOf: config => float;
  let config_of_json: StoreJson.json => config;
  let decodePatch: StoreJson.json => option(patch);
  let updateOfPatch: patch => config => config;
  let eventUrl: string;
  let baseUrl: string;
};

module type S = {
  type config;
  type patch;

  let subscribe: (~source: StoreSource.actions(config), ~config: config) => unit;
  let source: config => config;
};

module Make = (Schema: Schema) => {
  type config = Schema.config;
  type patch = Schema.patch;

  let%browser_only subscribe =
                   (
                     ~source: StoreSource.actions(config),
                     ~config: config,
                   ) => {
    switch (Schema.subscriptionOfConfig(config)) {
    | Some(subscription) =>
      RealtimeClient.Socket.subscribe(
        ~source,
        ~subscription=Schema.encodeSubscription(subscription),
        ~updatedAt=Schema.updatedAtOf(config),
        ~decodePatch=Schema.decodePatch,
        ~updateOfPatch=Schema.updateOfPatch,
        ~decodeSnapshot=Schema.config_of_json,
        ~updatedAtOf=Schema.updatedAtOf,
        ~eventUrl=Schema.eventUrl,
        ~baseUrl=Schema.baseUrl,
      )
    | None => ()
    };
  };

  let source = (config: config) =>
    switch%platform (Runtime.platform) {
    | Client =>
      let configSource =
        StoreSource.make(
          ~mount=source =>
            subscribe(
              ~source,
              ~config,
            ),
          config,
        );
      configSource.value;
    | Server => config
    };
};
