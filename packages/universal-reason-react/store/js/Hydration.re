type element;

[@platform native]
let getElementById = _id => Js.Nullable.null;
[@platform js]
[@mel.scope "document"] external getElementById: string => Js.Nullable.t(element) = "getElementById";

[@platform native]
let textContent = _element => Js.Nullable.null;
[@platform js]
[@mel.get] external textContent: element => Js.Nullable.t(string) = "textContent";

let parseState = (~stateElementId: string, ~decodeState: Js.Json.t => 'state) =>
  switch%platform (Runtime.platform) {
  | Client =>
    try({
      switch (stateElementId->getElementById->Js.Nullable.toOption) {
      | Some(element) =>
        switch (element->textContent->Js.Nullable.toOption) {
        | Some(text) => Some(text->Js.Json.parseExn->decodeState)
        | None => None
        }
      | None => None
      };
    }) {
    | _ => None
    }
  | Server =>
      let _ = stateElementId;
      let _ = decodeState;
      None
  };

let hydrateStore = (
  ~emptyStore: 'store,
  ~makeStore: 'state => 'store,
  ~decodeState: Js.Json.t => 'state,
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
