module type Schema = {
  type payload;
  type store;

  let emptyStore: store;
  let makeStore: payload => store;
  let payload_of_json: StoreJson.json => payload;
  let payload_to_json: payload => StoreJson.json;
  let stateElementId: string;
};

module Make = (Schema: Schema) => {
  type payload = Schema.payload;
  type store = Schema.store;

  let hydrateStore = () =>
    Hydration.hydrateStore(
      ~emptyStore=Schema.emptyStore,
      ~makeStore=Schema.makeStore,
      ~decodeState=Schema.payload_of_json,
      ~stateElementId=Schema.stateElementId,
    );

  let serializeState = (payload: payload) => {
    let _ = payload;
    switch%platform (Runtime.platform) {
    | Server => StoreJson.stringify(Schema.payload_to_json, payload)
    | Client => ""
    };
  };
};
