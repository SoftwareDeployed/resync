open Js.Promise;

[@mel.scope "process"]
external exitProcess: int => unit = "exit";

[@mel.send]
external includes: (string, string) => bool = "includes";

[@mel.new]
external makeError: string => exn = "Error";

[@mel.module "node:timers/promises"]
external sleep: int => Js.Promise.t(unit) = "setTimeout";

let bodyText = page =>
  page
  ->Playwright.textContent("body")
  |> then_(text =>
       resolve(
         switch (Js.Nullable.toOption(text)) {
         | Some(value) => value
         | None => ""
         },
       )
     );

let assertContains = (~label, ~expected, text) => {
  if (text->includes(expected)) {
    Js.log("[PASS] " ++ label);
    resolve();
  } else {
    reject(makeError(label ++ " missing expected text: " ++ expected));
  };
};

let assertNotContains = (~label, ~unexpected, text) => {
  if (!(text->includes(unexpected))) {
    Js.log("[PASS] " ++ label);
    resolve();
  } else {
    reject(makeError(label ++ " unexpectedly contains text: " ++ unexpected));
  };
};

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());

  Playwright.chromium
  ->Playwright.launch(launchOptions)
  |> then_(browser =>
       browser
       ->Playwright.newPage
       |> then_(page =>
            page
            ->Playwright.goto("http://127.0.0.1:8080/")
            |> then_(_ => bodyText(page))
             |> then_(text =>
                  assertContains(~label="SSR-first render: seeded content visible immediately", ~expected="Learn ReasonML", text)
                  |> then_(_ => assertContains(~label="SSR-first render: stats visible immediately", ~expected="0 of 3 completed", text))
                  |> then_(_ => assertNotContains(~label="No loading placeholder on SSR-first render", ~unexpected="Loading", text))
                )
             |> then_(_ => page->Playwright.fill(".todo-input", "Store package browser test"))
             |> then_(_ => page->Playwright.click(".todo-button"))
             |> then_(_ => page->Playwright.waitForSelector("text=Store package browser test"))
             |> then_(_ => bodyText(page))
             |> then_(text =>
                  assertContains(~label="Added todo persists in UI", ~expected="Store package browser test", text)
                  |> then_(_ => assertContains(~label="Stats after add", ~expected="0 of 4 completed", text))
                  |> then_(_ => assertNotContains(~label="No loading placeholder after add", ~unexpected="Loading", text))
                )
             |> then_(_ => page->Playwright.check(".todo-checkbox"))
             |> then_(_ => bodyText(page))
             |> then_(text =>
                  assertContains(~label="Stats after toggle", ~expected="1 of 4 completed", text)
                  |> then_(_ => assertNotContains(~label="No loading placeholder after toggle", ~unexpected="Loading", text))
                )
             |> then_(_ => sleep(300))
             |> then_(_ => {
                   Js.log("Reloading page to verify cache reconciliation...");
                   page->Playwright.reload;
                 })
             |> then_(_ => bodyText(page))
            |> then_(text =>
                 assertContains(~label="Cache reconciliation: added todo survives reload", ~expected="Store package browser test", text)
                 |> then_(_ => assertContains(~label="Cache reconciliation: stats survive reload", ~expected="1 of 4 completed", text))
                 |> then_(_ => assertNotContains(~label="No loading placeholder after reload", ~unexpected="Loading", text))
               )
            |> then_(_ => browser->Playwright.close)
          )
       );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Store browser tests passed!");
       exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Store browser tests failed:", error);
       exitProcess(1);
       resolve();
     })
  |> ignore;
