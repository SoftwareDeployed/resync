type element;

[@mel.scope "document"]
external getElementById: string => Js.Nullable.t(element) = "getElementById";

[@mel.get]
external textContent: element => Js.Nullable.t(string) = "textContent";

let parseState =
    (~stateElementId: string, ~decodeState: StoreJson.json => 'state) =>
  switch%platform (Runtime.platform) {
  | Client =>
    switch (stateElementId->getElementById->Js.Nullable.toOption) {
    | Some(element) =>
      switch (element->textContent->Js.Nullable.toOption) {
      | Some(text) => StoreJson.tryDecodeString(decodeState, text)
      | None => None
      }
    | None => None
    }
  | Server =>
    let _ = stateElementId;
    let _ = decodeState;
    None;
  };

let hydrateStore =
    (
      ~emptyStore: 'store,
      ~makeStore: 'state => 'store,
      ~decodeState: StoreJson.json => 'state,
      ~stateElementId: string,
    ) =>
  switch%platform (Runtime.platform) {
  | Client =>
    switch (parseState(~stateElementId, ~decodeState)) {
    | None => emptyStore
    | Some(state) => makeStore(state)
    }
  | Server => emptyStore
  };
