open Js.Promise;

let rec waitForSelectorText = (~page, ~selector, ~expected, ~label, ~attemptsLeft) =>
  if (attemptsLeft <= 0) {
    BrowserTestUtils.textOrEmpty(page, selector)
    |> then_(text =>
         reject(
           BrowserTestUtils.makeError(
             label
             ++ " timed out waiting for selector "
             ++ selector
             ++ " to contain: "
             ++ expected
             ++ ". Last value: "
             ++ text,
           ),
         )
       );
  } else {
    BrowserTestUtils.textOrEmpty(page, selector)
    |> then_(text =>
         if (text->BrowserTestUtils.includes(expected)) {
           Js.log("[PASS] " ++ label);
           resolve();
         } else {
           BrowserTestUtils.sleep(100)
           |> then_(_ =>
                waitForSelectorText(~page, ~selector, ~expected, ~label, ~attemptsLeft=attemptsLeft - 1)
              );
         }
       );
  };

let cleanup = (~browser, ~server) => {
  let closeBrowser =
    switch (browser) {
    | Some(activeBrowser) => activeBrowser->Playwright.close |> catch(_ => resolve())
    | None => resolve()
    };
  closeBrowser
  |> then_(_ =>
       switch (server) {
       | Some(activeServer) => TodoMultiplayerTestServer.stop(activeServer) |> catch(_ => resolve())
       | None => resolve()
       }
     );
};

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let browserRef = ref(None);
  let serverRef = ref(None);

  TodoMultiplayerTestServer.start()
  |> then_(server => {
       serverRef := Some(server);
       Playwright.chromium->Playwright.launch(launchOptions);
     })
  |> then_(browser => {
       browserRef := Some(browser);
       let baseUrl =
         switch (serverRef.contents) {
         | Some(s) => s.baseUrl
         | None => "http://127.0.0.1:9876"
         };

        browser
        ->Playwright.newPage
        |> then_(page =>
              page
              ->Playwright.goto(baseUrl ++ "/")
              |> then_(_ => page->Playwright.waitForSelector(".todo-container"))
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
            |> then_(text =>
                 BrowserTestUtils.assertContains(~label="SSR heading visible", ~expected="My Todo List", text)
                 |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder on initial load", ~unexpected="Loading", text))
               )
            |> then_(_ => page->Playwright.fill("input[type='text']", "Realtime browser test todo"))
            |> then_(_ => page->Playwright.click("button[type='submit']"))
            |> then_(_ => page->Playwright.waitForSelector("text=Realtime browser test todo"))
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
            |> then_(text =>
                 BrowserTestUtils.assertContains(~label="Optimistic add visible immediately", ~expected="Realtime browser test todo", text)
               )
             |> then_(_ => BrowserTestUtils.sleep(500))
             |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
             |> then_(text =>
                  BrowserTestUtils.assertContains(~label="Todo remains after websocket confirmation", ~expected="Realtime browser test todo", text)
                )
              |> then_(_ => page->Playwright.reload)
             |> then_(_ => page->Playwright.waitForSelector(".todo-container"))
             |> then_(_ => waitForSelectorText(~page, ~selector="body", ~expected="Realtime browser test todo", ~label="Cache replay: todo survives reload", ~attemptsLeft=50))
             |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
             |> then_(text =>
                  BrowserTestUtils.assertNotContains(~label="No loading placeholder after reload", ~unexpected="Loading", text)
                )
          );
     })
  |> then_(result =>
       cleanup(~browser=browserRef.contents, ~server=serverRef.contents)
       |> then_(_ => resolve(result))
     )
  |> catch(error =>
       cleanup(~browser=browserRef.contents, ~server=serverRef.contents)
       |> then_(_ => reject(Obj.magic(error)))
     );
};

let () =
  run()
  |> then_(_ => {
        Js.log("Todo multiplayer browser tests passed!");
        BrowserTestUtils.exitProcess(0);
       resolve();
     })
  |> catch(error => {
        Js.log2("Todo multiplayer browser tests failed:", error);
        BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
