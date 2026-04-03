type storage;

[@platform js] [@mel.scope "window"]
external localStorage: storage = "localStorage";

[@platform js] [@mel.send]
external getItem: (storage, string) => Js.Nullable.t(string) = "getItem";

[@platform js] [@mel.send]
external setItem: (storage, string, string) => unit = "setItem";

[@platform js] [@mel.send]
external removeItem: (storage, string) => unit = "removeItem";

module type Adapter = {
  let read: string => option(string);
  let write: (~key: string, ~value: string) => unit;
  let clear: string => unit;
};

module LocalStorageAdapter = {
  [@platform js]
  let read = key =>
    try(localStorage->getItem(key)->Js.Nullable.toOption) {
    | _ => None
    };

  [@platform native]
  let read = _key => None;

  [@platform js]
  let write = (~key, ~value) =>
    try(localStorage->setItem(key, value)) {
    | _ => ()
    };

  [@platform native]
  let write = (~key, ~value) => {
    let _ = key;
    let _ = value;
    ();
  };

  [@platform js]
  let clear = key =>
    try(localStorage->removeItem(key)) {
    | _ => ()
    };

  [@platform native]
  let clear = key => {
    let _ = key;
    ();
  };
};

module type Schema = {
  type state;
  type payload;

  module Adapter: Adapter;

  let storageKeyOfState: state => string;
  let payloadOfState: state => payload;
  let stateOfPayload: payload => state;
  let payload_of_json: StoreJson.json => payload;
  let payload_to_json: payload => StoreJson.json;
};

module type S = {
  type state;
  type payload;

  let hooks: StoreLayer.hooks(state);
  let readPayload: state => option(payload);
  let readState: state => option(state);
  let persistPayload: (~state: state, payload) => unit;
  let persistState: state => unit;
  let clearCurrent: unit => unit;
  let get: unit => option(state);
  let set: state => unit;
  let update: (state => state) => unit;
};

module Make = (Schema: Schema) => {
  type state = Schema.state;
  type payload = Schema.payload;

  let actionsRef: ref(option(StoreSource.actions(state))) = ref(None);
  let lastKeyRef: ref(option(string)) = ref(None);

  let rememberKey = key => {
    lastKeyRef := Some(key);
    key;
  };

  let readPayload = (state: state) => {
    let key = rememberKey(Schema.storageKeyOfState(state));
    switch (Schema.Adapter.read(key)) {
    | Some(value) => StoreJson.tryDecodeString(Schema.payload_of_json, value)
    | None => None
    };
  };

  let readState = (state: state) =>
    switch (readPayload(state)) {
    | Some(payload) => Some(Schema.stateOfPayload(payload))
    | None => None
    };

  let persistPayload = (~state: state, payload: payload) => {
    let key = rememberKey(Schema.storageKeyOfState(state));
    Schema.Adapter.write(
      ~key,
      ~value=StoreJson.stringify(Schema.payload_to_json, payload),
    );
  };

  let persistState = (state: state) =>
    persistPayload(~state, Schema.payloadOfState(state));

  let clearCurrent = () =>
    switch (lastKeyRef.contents) {
    | Some(key) => Schema.Adapter.clear(key)
    | None => ()
    };

  let get = () =>
    switch (actionsRef.contents) {
    | Some(actions) => Some(actions.get())
    | None => None
    };

  let set = (state: state) =>
    switch (actionsRef.contents) {
    | Some(actions) => {
        let _ = rememberKey(Schema.storageKeyOfState(state));
        actions.set(state);
      }
    | None => ()
    };

  let update = reducer =>
    switch (actionsRef.contents) {
    | Some(actions) => actions.update(reducer)
    | None => ()
    };

  let hooks: StoreLayer.hooks(state) = {
    init: state =>
      switch%platform (Runtime.platform) {
      | Client =>
        switch (readState(state)) {
        | Some(next) => next
        | None => state
        }
      | Server => state
      },
    mount: actions => actionsRef := Some(actions),
    afterSet: state => persistState(state),
  };
};
