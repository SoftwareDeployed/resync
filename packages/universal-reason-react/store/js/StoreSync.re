module type Schema = {
  type state;
  type patch;
  type subscription;

  let subscriptionOfState: state => option(subscription);
  let encodeSubscription: subscription => string;
  let updatedAtOf: state => float;
  let state_of_json: StoreJson.json => state;
  let decodePatch: StoreJson.json => option(patch);
  let updateOfPatch: patch => state => state;
  let eventUrl: string;
  let baseUrl: string;
};

module type S = {
  type state;
  type patch;

  let subscribe: (~source: StoreSource.actions(state), ~state: state) => unit;
  let hooks: StoreLayer.hooks(state);
  let source: state => state;
};

module Make = (Schema: Schema) => {
  type state = Schema.state;
  type patch = Schema.patch;

  let%browser_only subscribe =
                    (
                     ~source: StoreSource.actions(state),
                     ~state: state,
                    ) => {
    switch (Schema.subscriptionOfState(state)) {
    | Some(subscription) =>
      RealtimeClient.Socket.subscribe(
        ~source,
        ~subscription=Schema.encodeSubscription(subscription),
        ~updatedAt=Schema.updatedAtOf(state),
        ~decodePatch=Schema.decodePatch,
        ~updateOfPatch=Schema.updateOfPatch,
        ~decodeSnapshot=Schema.state_of_json,
        ~updatedAtOf=Schema.updatedAtOf,
        ~eventUrl=Schema.eventUrl,
        ~baseUrl=Schema.baseUrl,
      )
    | None => ()
    };
  };

  let hooks: StoreLayer.hooks(state) = {
    init: state => state,
    mount:
      switch%platform (Runtime.platform) {
      | Client => source => subscribe(~source, ~state=source.get())
      | Server => _source => ()
      },
    afterSet: _state => (),
  };

  let source = (state: state) => StoreLayer.source(~layers=[|hooks|], state);
};
