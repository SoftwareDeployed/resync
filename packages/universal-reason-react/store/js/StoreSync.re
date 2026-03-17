module type Schema = {
  type config;
  type patch;
  type subscription;

  let subscriptionOfConfig: config => option(subscription);
  let encodeSubscription: subscription => string;
  let updatedAtOf: config => float;
  let config_of_json: StoreJson.json => config;
  let patch_of_json: StoreJson.json => patch;
  let applyPatch: (config, patch) => config;
  let eventUrl: string;
  let baseUrl: string;
};

module type S = {
  type config;
  type patch;

  let subscribe: (~set: config => unit, ~get: unit => config, ~config: config) => unit;
  let source: config => config;
};

module Make = (Schema: Schema) => {
  type config = Schema.config;
  type patch = Schema.patch;

  let parsePatch = json => StoreJson.tryDecode(Schema.patch_of_json, json);

  let subscribe = (~set: config => unit, ~get: unit => config, ~config: config) => {
    switch (Schema.subscriptionOfConfig(config)) {
    | Some(subscription) =>
      RealtimeClient.Socket.subscribe(
        ~set,
        ~get,
        ~subscription=Schema.encodeSubscription(subscription),
        ~updatedAt=Schema.updatedAtOf(config),
        ~parsePatch,
        ~applyPatch=Schema.applyPatch,
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
      let currentConfig = ref(config);
      Tilia.Core.source(. config, (. _config, set) => {
        let setConfig = nextConfig => {
          currentConfig := nextConfig;
          set(nextConfig);
        };

        subscribe(~set=setConfig, ~get=() => currentConfig.contents, ~config);
      })
    | Server => config
    };
};
