open Melange_json.Primitives;
open Store;

let jsonObject = fill => {
  let dict: Js.Dict.t(Json.json) = Js.Dict.empty();
  fill(dict);
  Json.Dict.to_json(json => json, dict);
};

let setJsonField = (dict, key, value) => dict->Js.Dict.set(key, value);
let setStringField = (dict, key, value) => setJsonField(dict, key, string_to_json(value));
let setIntField = (dict, key, value) => setJsonField(dict, key, int_to_json(value));
let setFloatField = (dict, key, value) => setJsonField(dict, key, float_to_json(value));

module LocalFixture = {
  type state = {
    count: int,
    updated_at: float,
  };

  type action =
    | Increment
    | Decrement;

  type store = {
    state: state,
  };

  let emptyState = {
    count: 0,
    updated_at: 0.0,
  };

  let reduce = (~state: state, ~action: action) => {
    switch (action) {
    | Increment => {count: state.count + 1, updated_at: Js.Date.now()}
    | Decrement => {count: state.count - 1, updated_at: Js.Date.now()}
    };
  };

  let state_of_json = json => {
    count: Json.requiredField(~json, ~fieldName="count", ~decode=int_of_json),
    updated_at:
      Json.requiredField(~json, ~fieldName="updated_at", ~decode=float_of_json),
  };

  let state_to_json = state =>
    jsonObject(dict => {
      setIntField(dict, "count", state.count);
      setFloatField(dict, "updated_at", state.updated_at);
    });

  let action_of_json = json => {
    let kind =
      Json.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
    switch (kind) {
    | "increment" => Increment
    | _ => Decrement
    };
  };

  let action_to_json = action => {
    switch (action) {
    | Increment => jsonObject(dict => setStringField(dict, "kind", "increment"))
    | Decrement => jsonObject(dict => setStringField(dict, "kind", "decrement"))
    };
  };

  let makeStore =
      (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
    state:
      StoreBuilder.current(
        ~derive?,
        ~client=state,
        ~server=() => state,
        (),
      ),
  };

  let schema: StoreFrp.Local.schema(state, action, store) = {
    storeName: "fixture.local",
    emptyState,
    reduce,
    state_of_json,
    state_to_json,
    action_of_json,
    action_to_json,
    makeStore,
    scopeKeyOfState: _state => "default",
    timestampOfState: state => state.updated_at,
    stateElementId: None,
  };

  let config = schema |> StoreFrp.Local.make |> StoreFrp.Local.withCache(`IndexedDB);

  module StoreDef =
    StoreFrp.Local.Build({
      type nonrec state = state;
      type nonrec action = action;
      type nonrec store = store;
      let config = config;
    });

  include (
    StoreDef:
      Runtime.Exports
        with type state := state
         and type action := action
         and type t := store
  );

  module Context = StoreDef.Context;
};

module CrudFixture = {
  type row = {
    id: string,
    name: string,
  };

  let row_of_json = json => {
    id: Json.requiredField(~json, ~fieldName="id", ~decode=string_of_json),
    name: Json.requiredField(~json, ~fieldName="name", ~decode=string_of_json),
  };

  let row_to_json = row =>
    jsonObject(dict => {
      setStringField(dict, "id", row.id);
      setStringField(dict, "name", row.name);
    });

  type state = {
    items: array(row),
    updated_at: float,
  };

  type action =
    | AddRow(row)
    | RemoveRow(string);

  type subscription = string;

  type store = {
    state: state,
  };

  let emptyState = {
    items: [||],
    updated_at: 0.0,
  };

  let reduce = (~state: state, ~action: action) => {
    switch (action) {
    | AddRow(row) => {
        items: Crud.upsert(~getId=(r: row) => r.id, state.items, row),
        updated_at: Js.Date.now(),
      }
    | RemoveRow(id) => {
        items: Crud.remove(~getId=(r: row) => r.id, state.items, id),
        updated_at: Js.Date.now(),
      }
    };
  };

  let setTimestamp = (~state: state, ~timestamp: float) => {
    ...state,
    updated_at: timestamp,
  };

  let state_of_json = json => {
    items:
      Json.requiredField(
        ~json,
        ~fieldName="items",
        ~decode=json => json |> Melange_json.Of_json.array(row_of_json),
      ),
    updated_at:
      Json.requiredField(~json, ~fieldName="updated_at", ~decode=float_of_json),
  };

  let state_to_json = state =>
    jsonObject(dict => {
      setJsonField(dict, "items", Melange_json.To_json.array(row_to_json)(state.items));
      setFloatField(dict, "updated_at", state.updated_at);
    });

  let action_of_json = json => {
    let kind =
      Json.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
    switch (kind) {
    | "add_row" =>
      AddRow({
        id: Json.requiredField(~json, ~fieldName="id", ~decode=string_of_json),
        name:
          Json.requiredField(~json, ~fieldName="name", ~decode=string_of_json),
      })
    | _ =>
      RemoveRow(Json.requiredField(~json, ~fieldName="id", ~decode=string_of_json))
    };
  };

  let action_to_json = action => {
    switch (action) {
    | AddRow(row) =>
      jsonObject(dict => {
        setStringField(dict, "kind", "add_row");
        setStringField(dict, "id", row.id);
        setStringField(dict, "name", row.name);
      })
    | RemoveRow(id) =>
      jsonObject(dict => {
        setStringField(dict, "kind", "remove_row");
        setStringField(dict, "id", id);
      })
    };
  };

  let makeStore =
      (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
    state:
      StoreBuilder.current(
        ~derive?,
        ~client=state,
        ~server=() => state,
        (),
      ),
  };

  let schema: StoreFrp.Crud.schema(state, action, store) = {
    storeName: "fixture.crud",
    emptyState,
    reduce,
    state_of_json,
    state_to_json,
    action_of_json,
    action_to_json,
    makeStore,
    scopeKeyOfState: _state => "default",
    timestampOfState: state => state.updated_at,
    setTimestamp,
    stateElementId: None,
  };

  let transport: Sync.transportConfig(state, subscription) = {
    subscriptionOfState: _state => Some("fixture-sub"),
    encodeSubscription: sub => sub,
    eventUrl: "/events",
    baseUrl: "http://localhost:8080",
  };

  let crudStrategy: Sync.crudStrategy(state, row) =
    Sync.crud(
      ~table="fixture_rows",
      ~decodeRow=row_of_json,
      ~getId=(r: row) => r.id,
      ~getItems=(state: state) => state.items,
      ~setItems=(state: state, items) => {...state, items},
    );

  let config =
    schema
    |> StoreFrp.Crud.make(~transport, ~strategy=crudStrategy)
    |> StoreFrp.Crud.withCache(`IndexedDB);
  module StoreDef =
    StoreFrp.Crud.Build({
      type nonrec state = state;
      type nonrec action = action;
      type nonrec store = store;
      type nonrec subscription = subscription;
      type nonrec row = row;
      let config = config;
    });

  include (
    StoreDef:
      Runtime.Exports
        with type state := state
         and type action := action
         and type t := store
  );

  module Context = StoreDef.Context;
};

module CustomSyncedFixture = {
  type state = {
    messages: array(string),
    updated_at: float,
  };

  type patch =
    | AddMessage(string);

  type action =
    | SendMessage(string);

  type subscription = string;

  type store = {
    state: state,
  };

  let emptyState = {
    messages: [||],
    updated_at: 0.0,
  };

  let reduce = (~state: state, ~action: action) => {
    switch (action) {
    | SendMessage(msg) => {
        messages: Js.Array.concat(~other=[|msg|], state.messages),
        updated_at: Js.Date.now(),
      }
    };
  };

  let setTimestamp = (~state: state, ~timestamp: float) => {
    ...state,
    updated_at: timestamp,
  };

  let state_of_json = json => {
    messages:
      Json.requiredField(
        ~json,
        ~fieldName="messages",
        ~decode=json => json |> Melange_json.Of_json.array(string_of_json),
      ),
    updated_at:
      Json.requiredField(~json, ~fieldName="updated_at", ~decode=float_of_json),
  };

  let state_to_json = state =>
    jsonObject(dict => {
      setJsonField(
        dict,
        "messages",
        Melange_json.To_json.array(string_to_json)(state.messages),
      );
      setFloatField(dict, "updated_at", state.updated_at);
    });

  let action_of_json = json => {
    let kind =
      Json.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
    switch (kind) {
    | "send_message" =>
      SendMessage(
        Json.requiredField(~json, ~fieldName="message", ~decode=string_of_json),
      )
    | _ => SendMessage("")
    };
  };

  let action_to_json = action => {
    switch (action) {
    | SendMessage(msg) =>
      jsonObject(dict => {
        setStringField(dict, "kind", "send_message");
        setStringField(dict, "message", msg);
      })
    };
  };

  let makeStore =
      (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
    state:
      StoreBuilder.current(
        ~derive?,
        ~client=state,
        ~server=() => state,
        (),
      ),
  };

  let schema: StoreFrp.Synced.schema(state, action, store) = {
    storeName: "fixture.synced.custom",
    emptyState,
    reduce,
    state_of_json,
    state_to_json,
    action_of_json,
    action_to_json,
    makeStore,
    scopeKeyOfState: _state => "default",
    timestampOfState: state => state.updated_at,
    setTimestamp,
    stateElementId: None,
  };

  let transport: Sync.transportConfig(state, subscription) = {
    subscriptionOfState: _state => Some("fixture-custom-sub"),
    encodeSubscription: sub => sub,
    eventUrl: "/events",
    baseUrl: "http://localhost:8080",
  };

  let customStrategy: Sync.customStrategy(state, patch) =
    Sync.custom(
      ~decodePatch=json => {
        let kind =
          Json.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
        switch (kind) {
        | "add_message" =>
          Some(
            AddMessage(
              Json.requiredField(
                ~json,
                ~fieldName="message",
                ~decode=string_of_json,
              ),
            ),
          )
        | _ => None
        };
      },
      ~updateOfPatch=(patch, state) => {
        switch (patch) {
        | AddMessage(msg) => {
            messages: Js.Array.concat(~other=[|msg|], state.messages),
            updated_at: Js.Date.now(),
          }
        }
      },
    );

  let config =
    schema
    |> StoreFrp.Synced.make(~transport, ~strategy=customStrategy)
    |> StoreFrp.Synced.withCache(`IndexedDB);

  module StoreDef =
    StoreFrp.Synced.Build({
      type nonrec state = state;
      type nonrec action = action;
      type nonrec store = store;
      type nonrec subscription = subscription;
      type nonrec patch = patch;
      let config = config;
    });

  include (
    StoreDef:
      Runtime.Exports
        with type state := state
         and type action := action
         and type t := store
  );

  module Context = StoreDef.Context;
};
