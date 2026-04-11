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
