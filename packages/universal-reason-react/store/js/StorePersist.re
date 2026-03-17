type storage;

[@platform native]
let localStorage = Obj.magic(());
[@platform js]
[@mel.scope "window"] external localStorage: storage = "localStorage";

[@platform native]
let getItem = (_storage, _key) => Js.Nullable.null;
[@platform js]
[@mel.send] external getItem: (storage, string) => Js.Nullable.t(string) = "getItem";

[@platform native]
let setItem = (_storage, _key, _value) => ();
[@platform js]
[@mel.send] external setItem: (storage, string, string) => unit = "setItem";

[@platform native]
let removeItem = (_storage, _key) => ();
[@platform js]
[@mel.send] external removeItem: (storage, string) => unit = "removeItem";

module type Schema = {
  type payload;
  type store;

  let storageKey: string;
  let emptyStore: store;
  let makeStore: payload => store;
  let payloadOfStore: store => payload;
  let payload_of_json: StoreJson.json => payload;
  let payload_to_json: payload => StoreJson.json;
};

module Make = (Schema: Schema) => {
  type payload = Schema.payload;
  type store = Schema.store;

  let readPayload = () =>
    switch%platform (Runtime.platform) {
    | Client =>
      try({
        switch (localStorage->getItem(Schema.storageKey)->Js.Nullable.toOption) {
        | Some(value) => StoreJson.tryDecodeString(Schema.payload_of_json, value)
        | None => None
        };
      }) {
      | _ => None
      }
    | Server => None
    };

  let hydrateStore = () =>
    switch (readPayload()) {
    | Some(payload) => Schema.makeStore(payload)
    | None => Schema.emptyStore
    };

  let persistPayload = (payload: payload) => {
    let _ = payload;
    switch%platform (Runtime.platform) {
    | Client =>
      try(
        localStorage->setItem(
          Schema.storageKey,
          StoreJson.stringify(Schema.payload_to_json, payload),
        ),
      ) {
      | _ => ()
      }
    | Server => ()
    };
  };

  let persistStore = (store: store) => store->Schema.payloadOfStore->persistPayload;

  let clear = () =>
    switch%platform (Runtime.platform) {
    | Client =>
      try(localStorage->removeItem(Schema.storageKey)) {
      | _ => ()
      }
    | Server => ()
    };
};
