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

[@platform native]
let openRaw = (_name: string, _version: int) => Obj.magic();

[@platform native]
let setOnsuccess = (_req: request, _cb) => ();

[@platform native]
let setOnerror = (_req: request, _cb) => ();

[@platform native]
let setOnupgradeneeded = (_req: request, _cb) => ();

[@platform native]
let resultAsDatabase = (_req: request) => Obj.magic();

[@platform native]
let resultAsNullable = (_req: request) => Js.Nullable.null;

[@platform native]
let resultError = (_req: request) => Obj.magic();

[@platform native]
let errorName = (_e: error) => "";

[@platform native]
let errorMessage = (_e: error) => "";

[@platform native]
let createObjectStore = (_db: database, _name: string, _keyPath: string) =>
  Obj.magic();

[@platform native]
let createIndex = (_store: objectStore, _name: string, _keyPath: string, _unique: bool) =>
  Obj.magic();

[@platform native]
let transaction = (_db: database, _storeNames: array(string), _mode: string) =>
  Obj.magic();

[@platform native]
let objectStore = (_tx: transaction, _name: string) => Obj.magic();

[@platform native]
let index = (_store: objectStore, _name: string) => Obj.magic();

[@platform native]
let getRaw = (_store: objectStore, _key: string) => Obj.magic();

[@platform native]
let putRaw = (_store: objectStore, _value) => Obj.magic();

[@platform native]
let deleteRaw = (_store: objectStore, _key: string) => Obj.magic();

[@platform native]
let getAllRaw = (_idx: index, _key: string) => Obj.magic();

[@platform native]
let objectStoreNames = (_db: database) => [||];

[@platform native]
let includes = (_arr: array(string), _name: string) => false;

[@platform native]
let setTxOncomplete = (_tx: transaction, _cb) => ();

[@platform native]
let setTxOnerror = (_tx: transaction, _cb) => ();

[@platform native]
let txError = (_tx: transaction) => Obj.magic();

/* ============================================================================
 * JS EXTERNALS
 * ============================================================================ */

/* Open database */
[@platform js]
[@mel.scope "indexedDB"]
external openRaw: (string, int) => request = "open";

/* Request event handlers */
[@platform js]
[@mel.set]
external setOnsuccess: (request, [@mel.uncurry] (unit => unit)) => unit = "onsuccess";

[@platform js]
[@mel.set]
external setOnerror: (request, [@mel.uncurry] (unit => unit)) => unit = "onerror";

[@platform js]
[@mel.set]
external setOnupgradeneeded: (request, [@mel.uncurry] (unit => unit)) => unit = "onupgradeneeded";

/* Request result accessors */
[@platform js]
[@mel.get]
external resultAsDatabase: request => database = "result";

[@platform js]
[@mel.get]
external resultAsNullable: request => Js.Nullable.t('a) = "result";

[@platform js]
[@mel.get]
external resultError: request => error = "error";

/* Error accessors */
[@platform js]
[@mel.get]
external errorName: error => string = "name";

[@platform js]
[@mel.get]
external errorMessage: error => string = "message";

/* Database operations */
[@platform js]
[@mel.send]
external createObjectStore: (database, string, Js.t({. keyPath: string})) => objectStore =
  "createObjectStore";

[@platform js]
[@mel.send]
external createIndex: (objectStore, string, string, Js.t({. unique: bool})) => index =
  "createIndex";

[@platform js]
[@mel.send]
external transaction: (database, array(string), string) => transaction = "transaction";

[@platform js]
[@mel.send]
external objectStore: (transaction, string) => objectStore = "objectStore";

[@platform js]
[@mel.send]
external index: (objectStore, string) => index = "index";

/* Store/Index request operations */
[@platform js]
[@mel.send]
external getRaw: (objectStore, string) => request = "get";

[@platform js]
[@mel.send]
external putRaw: (objectStore, 'a) => request = "put";

[@platform js]
[@mel.send]
external deleteRaw: (objectStore, string) => request = "delete";

[@platform js]
[@mel.send]
external getAllRaw: (index, string) => request = "getAll";

/* Database metadata */
[@platform js]
[@mel.get]
external objectStoreNames: database => Js.Array.t(string) = "objectStoreNames";

[@platform js]
[@mel.send]
external includes: (Js.Array.t(string), string) => bool = "includes";

/* Transaction event handlers */
[@platform js]
[@mel.set]
external setTxOncomplete: (transaction, [@mel.uncurry] (unit => unit)) => unit = "oncomplete";

[@platform js]
[@mel.set]
external setTxOnerror: (transaction, [@mel.uncurry] (unit => unit)) => unit = "onerror";

[@platform js]
[@mel.get]
external txError: transaction => error = "error";
