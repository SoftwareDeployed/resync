open Js.Promise;

[@mel.scope "process"]
external exitProcess: int => unit = "exit";

[@mel.send]
external includes: (string, string) => bool = "includes";

[@mel.new]
external makeError: string => exn = "Error";

let assertContains = (~label, ~expected, text) => {
  if (text->includes(expected)) {
    Js.log("[PASS] " ++ label);
    resolve();
  } else {
    reject(makeError(label ++ " missing expected text: " ++ expected));
  };
};

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
            |> then_(_ => page->Playwright.waitForSelector("#root"))
            |> then_(_ => textOrEmpty(page, "#initial-store"))
            |> then_(text =>
                 assertContains(~label="Serialized state script", ~expected="Learn ReasonML", text)
                 |> then_(_ => assertContains(~label="Serialized state includes app data", ~expected="Build an app", text))
               )
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Components browser tests passed!");
       exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Components browser tests failed:", error);
       exitProcess(1);
       resolve();
     })
  |> ignore;
