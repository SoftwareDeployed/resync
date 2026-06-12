open Melange_json.Primitives;
open Store;

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

  let state_of_json = json => {
    count: Json.requiredField(~json, ~fieldName="count", ~decode=int_of_json),
    updated_at:
      Json.requiredField(~json, ~fieldName="updated_at", ~decode=float_of_json),
  };

  let state_to_json = state =>
    Json.Object.make(dict => {
      Json.Object.setInt(dict, "count", state.count);
      Json.Object.setFloat(dict, "updated_at", state.updated_at);
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
    | Increment => Json.Object.make(dict => Json.Object.setString(dict, "kind", "increment"))
    | Decrement => Json.Object.make(dict => Json.Object.setString(dict, "kind", "decrement"))
    };
  };

  let schema: StoreFrp.Local.schema(state, action, store) = {
    storeName: "fixture.local",
    emptyState: {
      count: 0,
      updated_at: 0.0,
    },
    reduce:
      (~state: state, ~action: action) =>
        switch (action) {
        | Increment => {count: state.count + 1, updated_at: Js.Date.now()}
        | Decrement => {count: state.count - 1, updated_at: Js.Date.now()}
        },
    state_of_json,
    state_to_json,
    action_of_json,
    action_to_json,
    makeStore:
      (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
        state:
          StoreBuilder.current(
            ~derive?,
            ~client=state,
            ~server=() => state,
            (),
          ),
      },
    scopeKeyOfState: _state => "default",
    timestampOfState: state => state.updated_at,
    stateElementId: None,
  };

  let config = schema |> StoreFrp.Local.make;

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
    Json.Object.make(dict => {
      Json.Object.setString(dict, "id", row.id);
      Json.Object.setString(dict, "name", row.name);
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
    Json.Object.make(dict => {
      Json.Object.setJson(
        dict,
        "items",
        Melange_json.To_json.array(row_to_json)(state.items),
      );
      Json.Object.setFloat(dict, "updated_at", state.updated_at);
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
      Json.Object.make(dict => {
        Json.Object.setString(dict, "kind", "add_row");
        Json.Object.setString(dict, "id", row.id);
        Json.Object.setString(dict, "name", row.name);
      })
    | RemoveRow(id) =>
      Json.Object.make(dict => {
        Json.Object.setString(dict, "kind", "remove_row");
        Json.Object.setString(dict, "id", id);
      })
    };
  };

  let schema: StoreFrp.Crud.schema(state, action, store) = {
    storeName: "fixture.crud",
    emptyState: {
      items: [||],
      updated_at: 0.0,
    },
    reduce:
      (~state: state, ~action: action) =>
        switch (action) {
        | AddRow(row) => {
            items: Crud.upsert(~getId=(r: row) => r.id, state.items, row),
            updated_at: Js.Date.now(),
          }
        | RemoveRow(id) => {
            items: Crud.remove(~getId=(r: row) => r.id, state.items, id),
            updated_at: Js.Date.now(),
          }
        },
    state_of_json,
    state_to_json,
    action_of_json,
    action_to_json,
    makeStore:
      (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
        state:
          StoreBuilder.current(
            ~derive?,
            ~client=state,
            ~server=() => state,
            (),
          ),
      },
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
    |> StoreFrp.Crud.make(~transport, ~strategy=crudStrategy);
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
    Json.Object.make(dict => {
      Json.Object.setJson(
        dict,
        "messages",
        Melange_json.To_json.array(string_to_json)(state.messages),
      );
      Json.Object.setFloat(dict, "updated_at", state.updated_at);
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
      Json.Object.make(dict => {
        Json.Object.setString(dict, "kind", "send_message");
        Json.Object.setString(dict, "message", msg);
      })
    };
  };

  let schema: StoreFrp.Synced.schema(state, action, store) = {
    storeName: "fixture.synced.custom",
    emptyState: {
      messages: [||],
      updated_at: 0.0,
    },
    reduce:
      (~state: state, ~action: action) =>
        switch (action) {
        | SendMessage(msg) => {
            messages: state.messages->Js.Array.concat(~other=[|msg|]),
            updated_at: Js.Date.now(),
          }
        },
    state_of_json,
    state_to_json,
    action_of_json,
    action_to_json,
    makeStore:
      (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
        state:
          StoreBuilder.current(
            ~derive?,
            ~client=state,
            ~server=() => state,
            (),
          ),
      },
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
            messages: state.messages->Js.Array.concat(~other=[|msg|]),
            updated_at: Js.Date.now(),
          }
        }
      },
    );

  let config =
    schema
    |> StoreFrp.Synced.make(~transport, ~strategy=customStrategy);

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
