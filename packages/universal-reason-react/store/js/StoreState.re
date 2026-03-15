module type Schema = {
  type payload;
  type store;

  let emptyStore: store;
  let makeStore: payload => store;
  let decodeState: Js.Json.t => payload;
  let encodeState: payload => string;
  let stateElementId: string;
};

module Make = (Schema: Schema) => {
  type payload = Schema.payload;
  type store = Schema.store;

  let hydrateStore = () =>
    Hydration.hydrateStore(
      ~emptyStore=Schema.emptyStore,
      ~makeStore=Schema.makeStore,
      ~decodeState=Schema.decodeState,
      ~stateElementId=Schema.stateElementId,
    );

  let serializeState = (payload: payload) => {
    let _ = payload;
    switch%platform (Runtime.platform) {
    | Server => Schema.encodeState(payload)
    | Client => ""
    };
  };
};
