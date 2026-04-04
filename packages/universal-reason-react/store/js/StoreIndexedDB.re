type database;

type state_record = {
  scopeKey: string,
  value: StoreJson.json,
  timestamp: float,
};

type action_record = {
  id: string,
  scopeKey: string,
  action: StoreJson.json,
  status: string,
  enqueuedAt: float,
  retryCount: int,
  error: option(string),
};

[@platform js]
let openDbJs: string => Js.Promise.t(database) =
  [%raw
    {|
  function(name) {
    return new Promise(function(resolve, reject) {
      var request = indexedDB.open(name, 1);
      request.onupgradeneeded = function(event) {
        var db = event.target.result;
        if (!db.objectStoreNames.contains("confirmed_state")) {
          db.createObjectStore("confirmed_state", { keyPath: "scopeKey" });
        }
        if (!db.objectStoreNames.contains("actions")) {
          var store = db.createObjectStore("actions", { keyPath: "id" });
          store.createIndex("scopeKey", "scopeKey", { unique: false });
        }
      };
      request.onsuccess = function(event) { resolve(event.target.result); };
      request.onerror = function(event) { reject(event.target.error); };
    });
  }
  |}];

[@platform native]
let openDbJs = (_name: string) => Js.Promise.resolve(Obj.magic(()));

[@platform js]
let getStateJs: (database, string) => Js.Promise.t(Js.Nullable.t(state_record)) =
  [%raw
    {|
  function(db, scopeKey) {
    return new Promise(function(resolve, reject) {
      var tx = db.transaction("confirmed_state", "readonly");
      var store = tx.objectStore("confirmed_state");
      var request = store.get(scopeKey);
      request.onsuccess = function(event) { resolve(event.target.result || null); };
      request.onerror = function(event) { reject(event.target.error); };
    });
  }
  |}];

[@platform native]
let getStateJs = (_db, _scopeKey) => Js.Promise.resolve(Js.Nullable.null);

[@platform js]
let setStateJs: (database, state_record) => Js.Promise.t(unit) =
  [%raw
    {|
  function(db, record) {
    return new Promise(function(resolve, reject) {
      var tx = db.transaction("confirmed_state", "readwrite");
      tx.objectStore("confirmed_state").put(record);
      tx.oncomplete = function() { resolve(); };
      tx.onerror = function(event) { reject(event.target.error); };
    });
  }
  |}];

[@platform native]
let setStateJs = (_db, _record) => Js.Promise.resolve();

[@platform js]
let getActionJs: (database, string) => Js.Promise.t(Js.Nullable.t(action_record)) =
  [%raw
    {|
  function(db, id) {
    return new Promise(function(resolve, reject) {
      var tx = db.transaction("actions", "readonly");
      var request = tx.objectStore("actions").get(id);
      request.onsuccess = function(event) { resolve(event.target.result || null); };
      request.onerror = function(event) { reject(event.target.error); };
    });
  }
  |}];

[@platform native]
let getActionJs = (_db, _id) => Js.Promise.resolve(Js.Nullable.null);

[@platform js]
let putActionJs: (database, action_record) => Js.Promise.t(unit) =
  [%raw
    {|
  function(db, record) {
    return new Promise(function(resolve, reject) {
      var tx = db.transaction("actions", "readwrite");
      tx.objectStore("actions").put(record);
      tx.oncomplete = function() { resolve(); };
      tx.onerror = function(event) { reject(event.target.error); };
    });
  }
  |}];

[@platform native]
let putActionJs = (_db, _record) => Js.Promise.resolve();

[@platform js]
let getActionsByScopeJs: (database, string) => Js.Promise.t(array(action_record)) =
  [%raw
    {|
  function(db, scopeKey) {
    return new Promise(function(resolve, reject) {
      var tx = db.transaction("actions", "readonly");
      var index = tx.objectStore("actions").index("scopeKey");
      var request = index.getAll(scopeKey);
      request.onsuccess = function(event) { resolve(event.target.result || []); };
      request.onerror = function(event) { reject(event.target.error); };
    });
  }
  |}];

[@platform native]
let getActionsByScopeJs = (_db, _scopeKey) => Js.Promise.resolve([||]);

[@platform js]
let deleteActionsJs: (database, array(string)) => Js.Promise.t(unit) =
  [%raw
    {|
  function(db, ids) {
    return new Promise(function(resolve, reject) {
      var tx = db.transaction("actions", "readwrite");
      var store = tx.objectStore("actions");
      for (var i = 0; i < ids.length; i++) {
        store.delete(ids[i]);
      }
      tx.oncomplete = function() { resolve(); };
      tx.onerror = function(event) { reject(event.target.error); };
    });
  }
  |}];

[@platform native]
let deleteActionsJs = (_db, _ids) => Js.Promise.resolve();

let dbsRef: ref(Js.Dict.t(database)) = ref(Js.Dict.empty());

let ensureOpen = (~name: string, ()) =>
  switch (dbsRef.contents->Js.Dict.get(name)) {
  | Some(db) => Js.Promise.resolve(db)
  | None =>
    Js.Promise.then_(
      db => {
        dbsRef.contents->Js.Dict.set(name, db);
        Js.Promise.resolve(db);
      },
      openDbJs(name),
    )
  };

let getState = (~name: string, ~scopeKey: string, ()) =>
  Js.Promise.then_(
    db =>
      Js.Promise.then_(
        result => Js.Promise.resolve(result->Js.Nullable.toOption),
        getStateJs(db, scopeKey),
      ),
    ensureOpen(~name, ()),
  );

let setState = (~name: string, record: state_record) =>
  Js.Promise.then_(db => setStateJs(db, record), ensureOpen(~name, ()));

let getAction = (~name: string, ~id: string, ()) =>
  Js.Promise.then_(
    db =>
      Js.Promise.then_(
        result => Js.Promise.resolve(result->Js.Nullable.toOption),
        getActionJs(db, id),
      ),
    ensureOpen(~name, ()),
  );

let putAction = (~name: string, record: action_record) =>
  Js.Promise.then_(db => putActionJs(db, record), ensureOpen(~name, ()));

let getActionsByScope = (~name: string, ~scopeKey: string, ()) =>
  Js.Promise.then_(
    db => getActionsByScopeJs(db, scopeKey),
    ensureOpen(~name, ()),
  );

let deleteActions = (~name: string, ~ids: array(string), ()) =>
  Js.Promise.then_(db => deleteActionsJs(db, ids), ensureOpen(~name, ()));
