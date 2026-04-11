open Js.Promise;

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
             |> then_(_ => BrowserTestUtils.waitForIDBContent(page, ~dbName="todo-multiplayer", ~expectedText="Realtime browser test todo"))
             |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
               |> then_(text =>
                    BrowserTestUtils.assertContains(~label="Todo remains after websocket confirmation", ~expected="Realtime browser test todo", text)
                  )
                |> then_(_ => page->Playwright.click("text=Fail Query"))
               |> then_(_ => page->Playwright.waitForSelector("text=Server failure test todo"))
               |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
               |> then_(text =>
                    BrowserTestUtils.assertContains(~label="Optimistic fail-server todo appears", ~expected="Server failure test todo", text)
                  )
               |> then_(_ => BrowserTestUtils.waitForBodyNotContains(page, ~unexpectedText="Server failure test todo"))
               |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
               |> then_(text =>
                    BrowserTestUtils.assertNotContains(~label="Optimistic fail-server todo rolled back", ~unexpected="Server failure test todo", text)
                  )
               |> then_(_ => page->Playwright.reload)
              |> then_(_ => page->Playwright.waitForSelector(".todo-container"))
              |> then_(_ => BrowserTestUtils.waitForIDBContent(page, ~dbName="todo-multiplayer", ~expectedText="Realtime browser test todo"))
              |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
              |> then_(text =>
                   BrowserTestUtils.assertContains(~label="Cache replay: todo survives reload", ~expected="Realtime browser test todo", text)
                 )
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
