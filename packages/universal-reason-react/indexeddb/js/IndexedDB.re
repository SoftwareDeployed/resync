/* IndexedDB API bindings for Melange - zero cost externals only */

/* Core types */
type database;
type transaction;
type objectStore;
type request;
type index;
type error;

/* ============================================================================
 * NATIVE STUBS
 * ============================================================================ */

[@native_only]
let unavailable = () =>
  raise(Failure("IndexedDB raw bindings are only available in the browser"));

[@native_only]
let openRaw = (_name: string, _version: int) => unavailable();

[@native_only]
let setOnsuccess = (_req: request, _cb) => ();

[@native_only]
let setOnerror = (_req: request, _cb) => ();

[@native_only]
let setOnupgradeneeded = (_req: request, _cb) => ();

[@native_only]
let resultAsDatabase = (_req: request) => unavailable();

[@native_only]
let resultAsNullable = (_req: request) => Js.Nullable.null;

[@native_only]
let resultError = (_req: request) => unavailable();

[@native_only]
let errorName = (_e: error) => "";

[@native_only]
let errorMessage = (_e: error) => "";

[@native_only]
let createObjectStore = (_db: database, _name: string, _keyPath: string) =>
  unavailable();

[@native_only]
let createIndex = (_store: objectStore, _name: string, _keyPath: string, _unique: bool) =>
  unavailable();

[@native_only]
let transaction = (_db: database, _storeNames: array(string), _mode: string) =>
  unavailable();

[@native_only]
let objectStore = (_tx: transaction, _name: string) => unavailable();

[@native_only]
let index = (_store: objectStore, _name: string) => unavailable();

[@native_only]
let getRaw = (_store: objectStore, _key: string) => unavailable();

[@native_only]
let putRaw = (_store: objectStore, _value) => unavailable();

[@native_only]
let deleteRaw = (_store: objectStore, _key: string) => unavailable();

[@native_only]
let getAllRaw = (_idx: index, _key: string) => unavailable();

[@native_only]
let objectStoreNames = (_db: database) => [||];

[@native_only]
let includes = (_arr: array(string), _name: string) => false;

[@native_only]
let setTxOncomplete = (_tx: transaction, _cb) => ();

[@native_only]
let setTxOnerror = (_tx: transaction, _cb) => ();

[@native_only]
let txError = (_tx: transaction) => unavailable();

/* ============================================================================
 * JS EXTERNALS
 * ============================================================================ */

/* Open database */
[@melange_only]
[@mel.scope "indexedDB"]
external openRaw: (string, int) => request = "open";

/* Request event handlers */
[@melange_only]
[@mel.set]
external setOnsuccess: (request, [@mel.uncurry] (unit => unit)) => unit = "onsuccess";

[@melange_only]
[@mel.set]
external setOnerror: (request, [@mel.uncurry] (unit => unit)) => unit = "onerror";

[@melange_only]
[@mel.set]
external setOnupgradeneeded: (request, [@mel.uncurry] (unit => unit)) => unit = "onupgradeneeded";

/* Request result accessors */
[@melange_only]
[@mel.get]
external resultAsDatabase: request => database = "result";

[@melange_only]
[@mel.get]
external resultAsNullable: request => Js.Nullable.t('a) = "result";

[@melange_only]
[@mel.get]
external resultError: request => error = "error";

/* Error accessors */
[@melange_only]
[@mel.get]
external errorName: error => string = "name";

[@melange_only]
[@mel.get]
external errorMessage: error => string = "message";

/* Database operations */
[@melange_only]
[@mel.send]
external createObjectStore: (database, string, Js.t({. keyPath: string})) => objectStore =
  "createObjectStore";

[@melange_only]
[@mel.send]
external createIndex: (objectStore, string, string, Js.t({. unique: bool})) => index =
  "createIndex";

[@melange_only]
[@mel.send]
external transaction: (database, array(string), string) => transaction = "transaction";

[@melange_only]
[@mel.send]
external objectStore: (transaction, string) => objectStore = "objectStore";

[@melange_only]
[@mel.send]
external index: (objectStore, string) => index = "index";

/* Store/Index request operations */
[@melange_only]
[@mel.send]
external getRaw: (objectStore, string) => request = "get";

[@melange_only]
[@mel.send]
external putRaw: (objectStore, 'a) => request = "put";

[@melange_only]
[@mel.send]
external deleteRaw: (objectStore, string) => request = "delete";

[@melange_only]
[@mel.send]
external getAllRaw: (index, string) => request = "getAll";

/* Database metadata */
[@melange_only]
[@mel.get]
external objectStoreNames: database => Js.Array.t(string) = "objectStoreNames";

[@melange_only]
[@mel.send]
external includes: (Js.Array.t(string), string) => bool = "contains";

/* Transaction event handlers */
[@melange_only]
[@mel.set]
external setTxOncomplete: (transaction, [@mel.uncurry] (unit => unit)) => unit = "oncomplete";

[@melange_only]
[@mel.set]
external setTxOnerror: (transaction, [@mel.uncurry] (unit => unit)) => unit = "onerror";

[@melange_only]
[@mel.get]
external txError: transaction => error = "error";
