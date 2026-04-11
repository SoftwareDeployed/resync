open Js.Promise;

let assertEquals = (~label, ~expected, actual) =>
  if (actual == expected) {
    Js.log("[PASS] " ++ label);
    resolve();
  } else {
    reject(
      BrowserTestUtils.makeError(label ++ " expected " ++ expected ++ " but got " ++ actual),
    );
  };

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());

  Playwright.chromium->Playwright.launch(launchOptions)
  |> then_(browser =>
       browser->Playwright.newPage
       |> then_(page =>
            page->Playwright.goto("http://127.0.0.1:8080/")
            |> then_(_ => page->Playwright.title)
            |> then_(title =>
                 assertEquals(
                   ~label="Document title",
                   ~expected="Todo App",
                   title,
                 )
               )
            |> then_(_ =>
                 page->Playwright.goto("http://127.0.0.1:8080/does-not-exist")
               )
            |> then_(_ => BrowserTestUtils.bodyText(page))
            |> then_(text =>
                 BrowserTestUtils.assertContains(
                   ~label="Deep link load",
                   ~expected="My Todo List",
                   text,
                 )
               )
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Router browser tests passed!");
       BrowserTestUtils.exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Router browser tests failed:", error);
       BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
