open Js.Promise;

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
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "#initial-store"))
            |> then_(text =>
                 BrowserTestUtils.assertContains(~label="Serialized state script", ~expected="Learn ReasonML", text)
                 |> then_(_ => BrowserTestUtils.assertContains(~label="Serialized state includes app data", ~expected="Build an app", text))
               )
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Components browser tests passed!");
       BrowserTestUtils.exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Components browser tests failed:", error);
       BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
