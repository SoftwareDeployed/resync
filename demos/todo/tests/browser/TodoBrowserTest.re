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
             |> then_(_ => BrowserTestUtils.bodyText(page))
             |> then_(text =>
                  BrowserTestUtils.assertContains(~label="SSR heading", ~expected="My Todo List", text)
                  |> then_(_ => BrowserTestUtils.assertContains(~label="SSR todo 1", ~expected="Learn ReasonML", text))
                  |> then_(_ => BrowserTestUtils.assertContains(~label="SSR todo 2", ~expected="Build an app", text))
                  |> then_(_ => BrowserTestUtils.assertContains(~label="SSR todo 3", ~expected="Deploy to production", text))
                  |> then_(_ => BrowserTestUtils.assertContains(~label="SSR stats", ~expected="0 of 3 completed", text))
                )
             |> then_(_ => page->Playwright.fill(".todo-input", "Write browser tests"))
             |> then_(_ => page->Playwright.click(".todo-button"))
             |> then_(_ => page->Playwright.waitForSelector("text=Write browser tests"))
             |> then_(_ => BrowserTestUtils.bodyText(page))
             |> then_(text =>
                  BrowserTestUtils.assertContains(~label="Added todo", ~expected="Write browser tests", text)
                  |> then_(_ => BrowserTestUtils.assertContains(~label="Stats after add", ~expected="0 of 4 completed", text))
                )
             |> then_(_ => page->Playwright.check(".todo-checkbox"))
             |> then_(_ => BrowserTestUtils.bodyText(page))
             |> then_(text =>
                  BrowserTestUtils.assertContains(~label="Stats after toggle", ~expected="1 of 4 completed", text)
                )
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
        Js.log("All browser tests passed!");
        BrowserTestUtils.exitProcess(0);
       resolve();
     })
  |> catch(error => {
        Js.log2("Browser tests failed:", error);
        BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
