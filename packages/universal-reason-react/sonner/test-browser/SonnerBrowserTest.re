open Js.Promise;

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let url =
    "file://"
    ++ Playwright.cwd()
    ++ "/packages/universal-reason-react/sonner/test-browser/generated/index.html";

  Playwright.chromium
  ->Playwright.launch(launchOptions)
  |> then_(browser =>
       browser
       ->Playwright.newPage
       |> then_(page =>
            page
            ->Playwright.goto(url)
            |> then_(_ => page->Playwright.waitForSelector("text=Browser toast message"))
            |> then_(_ => {
                 Js.log("[PASS] Sonner toast rendered");
                 resolve();
               })
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Sonner browser tests passed!");
       BrowserTestUtils.exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Sonner browser tests failed:", error);
       BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
