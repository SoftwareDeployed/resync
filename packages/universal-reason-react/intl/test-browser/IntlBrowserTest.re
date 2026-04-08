open Js.Promise;

[@mel.scope "process"]
external exitProcess: int => unit = "exit";

[@mel.send]
external includes: (string, string) => bool = "includes";

[@mel.new]
external makeError: string => exn = "Error";

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let url =
    "file://"
    ++ Playwright.cwd()
    ++ "/packages/universal-reason-react/intl/test-browser/generated/index.html";

  Playwright.chromium
  ->Playwright.launch(launchOptions)
  |> then_(browser =>
       browser
       ->Playwright.newPage
       |> then_(page =>
            page
            ->Playwright.goto(url)
            |> then_(_ => page->Playwright.waitForSelector("#result"))
            |> then_(_ => page->Playwright.textContent("#result"))
            |> then_(text => {
                 let value =
                   switch (Js.Nullable.toOption(text)) {
                   | Some(found) => found
                   | None => ""
                   };

                 if (value->includes("$1,234.50")) {
                   Js.log("[PASS] Intl formatting rendered");
                   resolve();
                 } else {
                   reject(makeError("Intl formatting did not match expected browser output"));
                 };
               })
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Intl browser tests passed!");
       exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Intl browser tests failed:", error);
       exitProcess(1);
       resolve();
     })
  |> ignore;
