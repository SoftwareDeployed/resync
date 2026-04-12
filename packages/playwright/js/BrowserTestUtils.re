open Js.Promise;

[@mel.scope "process"]
external exitProcess: int => unit = "exit";

[@mel.send]
external includes: (string, string) => bool = "includes";

[@mel.new]
external makeError: string => exn = "Error";

[@mel.module "node:timers/promises"]
external sleep: int => Js.Promise.t(unit) = "setTimeout";

let assertTrue = (~label, condition, ~details) => {
  if (condition) {
    Js.log("[PASS] " ++ label);
    resolve();
  } else {
    reject(makeError(label ++ " failed: " ++ details));
  };
};

let assertContains = (~label, ~expected, text) =>
  assertTrue(~label, text->includes(expected), ~details="missing expected text: " ++ expected);

let assertNotContains = (~label, ~unexpected, text) =>
  assertTrue(~label, !(text->includes(unexpected)), ~details="unexpected text present: " ++ unexpected);

let textOrEmpty = (page, selector) =>
  page
  ->Playwright.textContent(selector)
  |> then_(text =>
       resolve(
         switch (Js.Nullable.toOption(text)) {
         | Some(value) => value
         | None => ""
         },
       )
     );

let bodyText = page => textOrEmpty(page, "body");

/* Verify data is persisted in IndexedDB by polling the confirmed_state object store.
   Uses page.evaluate() to run raw IDB queries in the browser context.
   Resolves when any confirmed_state record's JSON stringified value contains
   the expectedText. Polls every 50ms with a 10-second timeout.
   This is a SOUND way to verify persistence without modifying app code. */
let waitForIDBContent = (page: Playwright.page, ~dbName: string, ~expectedText: string) => {
  let jsCode =
    "new Promise(function(resolve, reject) {" ++
    "  var timeoutId = setTimeout(function() {" ++
    "    reject('Timed out waiting for IDB persistence in ' + '" ++ dbName ++ "');" ++
    "  }, 10000);" ++
    "  var poll = function() {" ++
    "    try {" ++
    "      var req = indexedDB.open('" ++ dbName ++ "', 2);" ++
    "      req.onsuccess = function(e) {" ++
    "        var db = e.target.result;" ++
    "        try {" ++
    "          var tx = db.transaction('confirmed_state', 'readonly');" ++
    "          var store = tx.objectStore('confirmed_state');" ++
    "          var getAllReq = store.getAll();" ++
    "          getAllReq.onsuccess = function() {" ++
    "            var found = false;" ++
    "            for (var i = 0; i < getAllReq.result.length; i++) {" ++
    "              var val = JSON.stringify(getAllReq.result[i].value);" ++
    "              if (val && val.indexOf('" ++ expectedText ++ "') !== -1) {" ++
    "                found = true;" ++
    "                break;" ++
    "              }" ++
    "            }" ++
    "            db.close();" ++
    "            if (found) { clearTimeout(timeoutId); resolve('ok'); }" ++
    "            else { setTimeout(poll, 50); }" ++
    "          };" ++
    "          getAllReq.onerror = function() { db.close(); setTimeout(poll, 50); };" ++
    "        } catch(err) { db.close(); setTimeout(poll, 50); }" ++
    "      };" ++
    "      req.onerror = function() { setTimeout(poll, 50); };" ++
    "    } catch(err) { setTimeout(poll, 50); }" ++
    "  };" ++
    "  poll();" ++
    "})";
  Playwright.evaluateString(page, jsCode);
};

/* Seed a confirmed_state record in IndexedDB before page navigation.
   Uses Playwright.addInitScript to inject data before any page JS runs.
   The record matches the real schema: {scopeKey, value, timestamp}. */
let seedConfirmedStateBeforeNavigation = (page: Playwright.page, ~dbName: string, ~scopeKey: string, ~timestamp: float, ~jsonValue: string) => {
  let jsCode =
    "window.__seedIDB = function() {"
    ++ "  return new Promise(function(resolve, reject) {"
    ++ "    var req = indexedDB.open('" ++ dbName ++ "', 2);"
    ++ "    req.onupgradeneeded = function(e) {"
    ++ "      var db = e.target.result;"
    ++ "      if (!db.objectStoreNames.contains('confirmed_state')) {"
    ++ "        db.createObjectStore('confirmed_state', {keyPath: 'scopeKey'});"
    ++ "      }"
    ++ "      if (!db.objectStoreNames.contains('actions')) {"
    ++ "        var store = db.createObjectStore('actions', {keyPath: 'id'});"
    ++ "        store.createIndex('scopeKey', 'scopeKey', {unique: false});"
    ++ "      }"
    ++ "    };"
    ++ "    req.onsuccess = function(e) {"
    ++ "      var db = e.target.result;"
    ++ "      var tx = db.transaction('confirmed_state', 'readwrite');"
    ++ "      var store = tx.objectStore('confirmed_state');"
    ++ "      store.put({"
    ++ "        scopeKey: '" ++ scopeKey ++ "',"
    ++ "        value: JSON.parse('" ++ jsonValue ++ "'),"
    ++ "        timestamp: " ++ string_of_float(timestamp)
    ++ "      });"
    ++ "      tx.oncomplete = function() { db.close(); resolve('seeded'); };"
    ++ "      tx.onerror = function() { db.close(); reject('seed failed'); };"
    ++ "    };"
    ++ "    req.onerror = function() { reject('open failed'); };"
    ++ "  });"
    ++ "};"
    ++ "window.__seedIDB().then(function() { console.log('IDB seeded for " ++ dbName ++ "'); }).catch(function(e) { console.log('IDB seed error: ' + e); });";
  Playwright.addInitScript(page, jsCode);
};

/* Wait until the page body contains the given text.
   Polls every 100ms with a 10-second timeout. */
let waitForBodyContains = (page: Playwright.page, ~expectedText: string) => {
  let jsCode =
    "new Promise(function(resolve, reject) {"
    ++ "  var timeoutId = setTimeout(function() {"
    ++ "    reject('Timed out waiting for text to appear: " ++ expectedText ++ "');"
    ++ "  }, 10000);"
    ++ "  var poll = function() {"
    ++ "    if (document.body.innerText.indexOf('" ++ expectedText ++ "') !== -1) {"
    ++ "      clearTimeout(timeoutId);"
    ++ "      resolve('ok');"
    ++ "    } else {"
    ++ "      setTimeout(poll, 100);"
    ++ "    }"
    ++ "  };"
    ++ "  poll();"
    ++ "})";
  Playwright.evaluateString(page, jsCode);
};

/* Wait until the page body no longer contains the given text.
   Polls every 100ms with a 10-second timeout. */
let waitForBodyNotContains = (page: Playwright.page, ~unexpectedText: string) => {
  let jsCode =
    "new Promise(function(resolve, reject) {"
    ++ "  var timeoutId = setTimeout(function() {"
    ++ "    reject('Timed out waiting for text to disappear: " ++ unexpectedText ++ "');"
    ++ "  }, 10000);"
    ++ "  var poll = function() {"
    ++ "    if (document.body.innerText.indexOf('" ++ unexpectedText ++ "') === -1) {"
    ++ "      clearTimeout(timeoutId);"
    ++ "      resolve('ok');"
    ++ "    } else {"
    ++ "      setTimeout(poll, 100);"
    ++ "    }"
    ++ "  };"
    ++ "  poll();"
    ++ "})";
  Playwright.evaluateString(page, jsCode);
};
