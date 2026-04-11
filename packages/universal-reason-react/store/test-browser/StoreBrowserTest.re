open Js.Promise;

/* Cleanup helper for browser and server resources */
let cleanup = (~browser, ~server) => {
  let closeBrowser =
    switch (browser) {
    | Some(activeBrowser) => activeBrowser->Playwright.close |> catch(_ => resolve())
    | None => resolve()
    };
  closeBrowser
  |> then_(_ =>
       switch (server) {
       | Some(activeServer) => StoreTestServer.stop(activeServer) |> catch(_ => resolve())
       | None => resolve()
       }
     );
};

/* Test 1: SSR render, add, toggle, IDB persistence, reload */
let testBasicFlow = (~page, ~baseUrl) => {
  page
  ->Playwright.goto(baseUrl ++ "/")
  |> then_(_ => BrowserTestUtils.bodyText(page))
  |> then_(text =>
       BrowserTestUtils.assertContains(~label="SSR-first render: seeded content visible immediately", ~expected="Learn ReasonML", text)
       |> then_(_ => BrowserTestUtils.assertContains(~label="SSR-first render: stats visible immediately", ~expected="0 of 3 completed", text))
       |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder on SSR-first render", ~unexpected="Loading", text))
     )
  |> then_(_ => page->Playwright.fill(".todo-input", "Store package browser test"))
  |> then_(_ => page->Playwright.click(".todo-button"))
  |> then_(_ => page->Playwright.waitForSelector("text=Store package browser test"))
  |> then_(_ => BrowserTestUtils.bodyText(page))
  |> then_(text =>
       BrowserTestUtils.assertContains(~label="Added todo persists in UI", ~expected="Store package browser test", text)
       |> then_(_ => BrowserTestUtils.assertContains(~label="Stats after add", ~expected="0 of 4 completed", text))
       |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder after add", ~unexpected="Loading", text))
     )
  |> then_(_ => page->Playwright.check(".todo-checkbox"))
  |> then_(_ => BrowserTestUtils.bodyText(page))
  |> then_(text =>
       BrowserTestUtils.assertContains(~label="Stats after toggle", ~expected="1 of 4 completed", text)
       |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder after toggle", ~unexpected="Loading", text))
     )
  |> then_(_ => BrowserTestUtils.waitForIDBContent(page, ~dbName="todo.simple", ~expectedText="Store package browser test"))
  |> then_(_ => {
       Js.log("Reloading page to verify cache reconciliation...");
       page->Playwright.reload;
     })
  |> then_(_ => BrowserTestUtils.bodyText(page))
  |> then_(text =>
       BrowserTestUtils.assertContains(~label="Cache reconciliation: added todo survives reload", ~expected="Store package browser test", text)
       |> then_(_ => BrowserTestUtils.assertContains(~label="Cache reconciliation: stats survive reload", ~expected="1 of 4 completed", text))
       |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder after reload", ~unexpected="Loading", text))
     );
};

/* Test 2: Hydration conflict — newer IDB state wins over SSR */
let testHydrationConflict = (~page) => {
  /* Seed IDB with a state that has timestamp 5000.0 (> SSR's 0.0) */
  BrowserTestUtils.seedConfirmedStateBeforeNavigation(
    page,
    ~dbName="todo.simple",
    ~scopeKey="default",
    ~timestamp=5000.0,
    ~jsonValue="{\"todos\":[{\"id\":\"idb-1\",\"text\":\"IDB wins over SSR\",\"completed\":true}],\"updated_at\":5000.0}",
  )
  |> then_(_ => page->Playwright.goto("http://127.0.0.1:8090/"))
  |> then_(_ => page->Playwright.waitForSelector(".todo-container"))
  |> then_(_ => BrowserTestUtils.bodyText(page))
  |> then_(text =>
       /* IDB state should win because timestamp 5000.0 > SSR's 0.0 */
       BrowserTestUtils.assertContains(~label="Hydration conflict: IDB state wins", ~expected="IDB wins over SSR", text)
       |> then_(_ => BrowserTestUtils.assertContains(~label="Hydration conflict: IDB stats correct", ~expected="1 of 1 completed", text))
       /* SSR todos should NOT be present — this proves wholesale swap, not merge */
       |> then_(_ => BrowserTestUtils.assertNotContains(~label="Hydration conflict: SSR todos replaced", ~unexpected="Learn ReasonML", text))
       |> then_(_ => BrowserTestUtils.assertNotContains(~label="Hydration conflict: SSR todos replaced", ~unexpected="Build an app", text))
       |> then_(_ => BrowserTestUtils.assertNotContains(~label="Hydration conflict: SSR todos replaced", ~unexpected="Deploy to production", text))
     );
};

/* Test 3: Cross-tab broadcast via BroadcastChannel */
let testCrossTabBroadcast = (~browser) => {
  browser
  ->Playwright.newContext
  |> then_(context =>
       /* Open first page */
       context
       ->Playwright.newPageInContext
       |> then_(pageA =>
            pageA
            ->Playwright.goto("http://127.0.0.1:8090/")
            |> then_(_ => pageA->Playwright.waitForSelector(".todo-container"))
            /* Open second page in same context (shared IDB + BroadcastChannel) */
            |> then_(_ => context->Playwright.newPageInContext)
            |> then_(pageB =>
                 pageB
                 ->Playwright.goto("http://127.0.0.1:8090/")
                 |> then_(_ => pageB->Playwright.waitForSelector(".todo-container"))
                 /* Modify state in page A */
                 |> then_(_ => pageA->Playwright.fill(".todo-input", "BroadcastChannel sync todo"))
                 |> then_(_ => pageA->Playwright.click(".todo-button"))
                 /* Wait for optimistic render on page A */
                 |> then_(_ => pageA->Playwright.waitForSelector("text=BroadcastChannel sync todo"))
                 /* Wait for IDB persistence so broadcast fires */
                 |> then_(_ => BrowserTestUtils.waitForIDBContent(pageA, ~dbName="todo.simple", ~expectedText="BroadcastChannel sync todo"))
                 /* Page B should receive the broadcast WITHOUT reload */
                 |> then_(_ => pageB->Playwright.waitForSelector("text=BroadcastChannel sync todo"))
                 |> then_(_ => BrowserTestUtils.bodyText(pageB))
                 |> then_(text =>
                      BrowserTestUtils.assertContains(~label="Cross-tab: page B received broadcast", ~expected="BroadcastChannel sync todo", text)
                      |> then_(_ => BrowserTestUtils.assertContains(~label="Cross-tab: page B stats correct", ~expected="0 of 4 completed", text))
                    )
                 |> then_(_ => context->Playwright.closeContext)
               )
          )
     );
};

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let browserRef = ref(None);
  let serverRef = ref(None);

  StoreTestServer.start()
  |> then_(server => {
       serverRef := Some(server);
       Playwright.chromium->Playwright.launch(launchOptions);
     })
  |> then_(browser => {
       browserRef := Some(browser);
       browser
       ->Playwright.newPage
       |> then_(page =>
            testBasicFlow(~page, ~baseUrl="http://127.0.0.1:8090")
            |> then_(_ => {
                 Js.log("Basic flow test passed. Starting hydration conflict test...");
               })
            |> then_(_ => browser->Playwright.newPage)
            |> then_(page2 =>
                 testHydrationConflict(~page=page2)
                 |> then_(_ => page2->Playwright.close)
               )
            |> then_(_ => {
                 Js.log("Hydration conflict test passed. Starting cross-tab broadcast test...");
               })
            |> then_(_ => testCrossTabBroadcast(~browser))
            |> then_(_ => browser->Playwright.close)
          )
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
        Js.log("Store browser tests passed!");
        BrowserTestUtils.exitProcess(0);
       resolve();
      })
  |> catch(error => {
        Js.log2("Store browser tests failed:", error);
        BrowserTestUtils.exitProcess(1);
       resolve();
      })
  |> ignore;
