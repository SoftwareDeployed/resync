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
            |> then_(_ => page->Playwright.waitForSelector(".todo-delete svg"))
            |> then_(_ => page->Playwright.waitForSelector(".todo-delete svg path"))
            |> then_(_ => {
                 Js.log("[PASS] Lucide icon SVG rendered");
                 resolve();
               })
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Lucide icons browser tests passed!");
       BrowserTestUtils.exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Lucide icons browser tests failed:", error);
       BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
