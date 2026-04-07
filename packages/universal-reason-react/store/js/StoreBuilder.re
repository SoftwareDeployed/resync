let derived = (~derive: option(Tilia.Core.deriver('store))=?, ~client, ~server, ()) =>
  switch (derive) {
  | Some(derive) => derive.derived(client)
  | None => server()
  };

let current = (~derive: option(Tilia.Core.deriver('store))=?, ~client, ~server, ()) =>
  switch (derive) {
  | Some(_) => client
  | None => server()
  };

let projected = (
  ~derive: option(Tilia.Core.deriver('store))=?,
  ~project,
  ~serverSource,
  ~fromStore,
  ~select,
  (),
) =>
  derived(
    ~derive?,
    ~client=store => select(project(fromStore(store))),
    ~server=() => select(project(serverSource)),
    (),
  );

module Runtime = {
  module type Exports = {
    type state;
    type action;
    type t;

    let empty: t;
    let hydrateStore: unit => t;
    let createStore: state => t;
    let serializeState: state => string;
    let serializeSnapshot: state => string;
    let dispatch: action => unit;
  };

  module type Schema = {
    type state;
    type action;
    type store;

    let reduce: (~state: state, ~action: action) => state;
    let emptyState: state;
    let storeName: string;
    let stateElementId: string;
    let scopeKeyOfState: state => string;
    let timestampOfState: state => float;
    let state_of_json: StoreJson.json => state;
    let state_to_json: state => StoreJson.json;
    let action_of_json: StoreJson.json => action;
    let action_to_json: action => StoreJson.json;
    let makeStore:
      (~state: state, ~derive: Tilia.Core.deriver(store)=?, unit) => store;
  };

  module Make = StoreOffline.Local.Make;

  module type SyncedSchema = {
    type state;
    type action;
    type store;
    type subscription;
    type patch;

    let reduce: (~state: state, ~action: action) => state;
    let emptyState: state;
    let storeName: string;
    let stateElementId: string;
    let scopeKeyOfState: state => string;
    let timestampOfState: state => float;
    let setTimestamp: (~state: state, ~timestamp: float) => state;
    let state_of_json: StoreJson.json => state;
    let state_to_json: state => StoreJson.json;
    let action_of_json: StoreJson.json => action;
    let action_to_json: action => StoreJson.json;
    let makeStore:
      (~state: state, ~derive: Tilia.Core.deriver(store)=?, unit) => store;
    let subscriptionOfState: state => option(subscription);
    let encodeSubscription: subscription => string;
let eventUrl: string;
let baseUrl: string;
let decodePatch: StoreJson.json => option(patch);
let updateOfPatch: patch => state => state;
let onActionError: string => unit;
let onCustom: option(StoreJson.json => unit);
let onMedia: option(StoreJson.json => unit);
let onOpen: option(unit => unit);
};

  module MakeSynced = StoreOffline.Synced.Make;
};
