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
type database = IndexedDB.database;

[@platform native]
type database = unit;

/* Open database with schema upgrade handler */
[@platform js]
let openDbJs = (name: string) => {
  Js.Promise.make((~resolve, ~reject) => {
    let req = IndexedDB.openRaw(name, 1);
    IndexedDB.setOnsuccess(req, () => resolve(. IndexedDB.resultAsDatabase(req)));
    IndexedDB.setOnerror(req, () =>
      reject(. Failure(
        "IndexedDB open error: "
        ++ IndexedDB.errorMessage(IndexedDB.resultError(req)),
      ))
    );
    IndexedDB.setOnupgradeneeded(req, () => {
      let db = IndexedDB.resultAsDatabase(req);
      let storeNames = IndexedDB.objectStoreNames(db);
      if (!IndexedDB.includes(storeNames, "confirmed_state")) {
        let _ = IndexedDB.createObjectStore(db, "confirmed_state", [%obj {keyPath: "scopeKey"}]);
        ()
      };
      if (!IndexedDB.includes(storeNames, "actions")) {
        let store = IndexedDB.createObjectStore(db, "actions", [%obj {keyPath: "id"}]);
        let _ = IndexedDB.createIndex(store, "scopeKey", "scopeKey", [%obj {unique: false}]);
        ()
      };
    });
  });
};

[@platform native]
let openDbJs = (_name: string) => Js.Promise.resolve(Obj.magic(()));

/* Get state record by scope key */
[@platform js]
let getStateJs = (db: database, scopeKey: string) => {
  let tx = IndexedDB.transaction(db, [|"confirmed_state"|], "readonly");
  let store = IndexedDB.objectStore(tx, "confirmed_state");
  Js.Promise.make((~resolve, ~reject) => {
    let req = IndexedDB.getRaw(store, scopeKey);
    IndexedDB.setOnsuccess(req, () => resolve(. IndexedDB.resultAsNullable(req)));
    IndexedDB.setOnerror(req, () => reject(. Failure("IndexedDB get error")));
  });
};

[@platform native]
let getStateJs = (_db, _scopeKey) => Js.Promise.resolve(Js.Nullable.null);

/* Save state record */
[@platform js]
let setStateJs = (db: database, record: state_record) => {
  Js.Promise.then_(
    _done => Js.Promise.resolve(),
    Js.Promise.make((~resolve, ~reject) => {
      let tx = IndexedDB.transaction(db, [|"confirmed_state"|], "readwrite");
      let store = IndexedDB.objectStore(tx, "confirmed_state");
      let _ = IndexedDB.putRaw(store, record);
      IndexedDB.setTxOncomplete(tx, () => resolve(. true));
      IndexedDB.setTxOnerror(tx, () =>
        reject(. Failure("IndexedDB transaction error"))
      );
    }),
  );
};

[@platform native]
let setStateJs = (_db, _record) => Js.Promise.resolve();

/* Get action record by id */
[@platform js]
let getActionJs = (db: database, id: string) => {
  let tx = IndexedDB.transaction(db, [|"actions"|], "readonly");
  let store = IndexedDB.objectStore(tx, "actions");
  Js.Promise.make((~resolve, ~reject) => {
    let req = IndexedDB.getRaw(store, id);
    IndexedDB.setOnsuccess(req, () => resolve(. IndexedDB.resultAsNullable(req)));
    IndexedDB.setOnerror(req, () => reject(. Failure("IndexedDB get error")));
  });
};

[@platform native]
let getActionJs = (_db, _id) => Js.Promise.resolve(Js.Nullable.null);

/* Save action record */
[@platform js]
let putActionJs = (db: database, record: action_record) => {
  Js.Promise.then_(
    _done => Js.Promise.resolve(),
    Js.Promise.make((~resolve, ~reject) => {
      let tx = IndexedDB.transaction(db, [|"actions"|], "readwrite");
      let store = IndexedDB.objectStore(tx, "actions");
      let _ = IndexedDB.putRaw(store, record);
      IndexedDB.setTxOncomplete(tx, () => resolve(. true));
      IndexedDB.setTxOnerror(tx, () =>
        reject(. Failure("IndexedDB transaction error"))
      );
    }),
  );
};

[@platform native]
let putActionJs = (_db, _record) => Js.Promise.resolve();

/* Get all actions for a scope */
[@platform js]
let getActionsByScopeJs = (db: database, scopeKey: string) => {
  let tx = IndexedDB.transaction(db, [|"actions"|], "readonly");
  let store = IndexedDB.objectStore(tx, "actions");
  let idx = IndexedDB.index(store, "scopeKey");
  Js.Promise.make((~resolve, ~reject) => {
    let req = IndexedDB.getAllRaw(idx, scopeKey);
    IndexedDB.setOnsuccess(req, () => {
      switch (Js.Nullable.toOption(IndexedDB.resultAsNullable(req))) {
      | Some(arr) => resolve(. arr)
      | None => resolve(. [||])
      };
    });
    IndexedDB.setOnerror(req, () => reject(. Failure("IndexedDB getAll error")));
  });
};

[@platform native]
let getActionsByScopeJs = (_db, _scopeKey) => Js.Promise.resolve([||]);

/* Delete multiple actions by id */
[@platform js]
let deleteActionsJs = (db: database, ids: array(string)) => {
  Js.Promise.then_(
    _done => Js.Promise.resolve(),
    Js.Promise.make((~resolve, ~reject) => {
      let tx = IndexedDB.transaction(db, [|"actions"|], "readwrite");
      let store = IndexedDB.objectStore(tx, "actions");
      Js.Array.forEach(
        ~f=id => {
          let _ = IndexedDB.deleteRaw(store, id);
          ()
        },
        ids,
      );
      IndexedDB.setTxOncomplete(tx, () => resolve(. true));
      IndexedDB.setTxOnerror(tx, () =>
        reject(. Failure("IndexedDB transaction error"))
      );
    }),
  );
};

[@platform native]
let deleteActionsJs = (_db, _ids) => Js.Promise.resolve();

/* Database cache */
let dbsRef: ref(Js.Dict.t(database)) = ref(Js.Dict.empty());

/* Ensure database is open */
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

/* Public API */
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
