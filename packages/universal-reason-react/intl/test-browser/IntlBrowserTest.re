open Js.Promise;

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
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "#result"))
            |> then_(value =>
                 BrowserTestUtils.assertContains(~label="Intl formatting rendered", ~expected="$1,234.50", value)
               )
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Intl browser tests passed!");
       BrowserTestUtils.exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Intl browser tests failed:", error);
       BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
