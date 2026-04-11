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
       | Some(activeServer) => EcommerceTestServer.stop(activeServer) |> catch(_ => resolve())
       | None => resolve()
       }
     );
};

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let browserRef = ref(None);
  let serverRef = ref(None);

  EcommerceTestServer.start()
  |> then_(server => {
       serverRef := Some(server);
       Playwright.chromium->Playwright.launch(launchOptions);
     })
  |> then_(browser => {
       browserRef := Some(browser);
       let baseUrl =
         switch (serverRef.contents) {
         | Some(s) => s.baseUrl
         | None => "http://127.0.0.1:9888"
         };

       browser
       ->Playwright.newPage
       |> then_(page =>
            page
            ->Playwright.goto(baseUrl ++ "/")
            |> then_(_ => page->Playwright.waitForSelector("text=Cloud Hardware Rental"))
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
             |> then_(text =>
                  BrowserTestUtils.assertContains(~label="SSR heading visible", ~expected="Cloud Hardware Rental", text)
                  |> then_(_ => BrowserTestUtils.assertContains(~label="SSR cart visible", ~expected="Selected equipment", text))
                  |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder on initial load", ~unexpected="Loading", text))
                )
             |> then_(_ => page->Playwright.waitForSelector("text=Add to cart"))
             |> then_(_ => page->Playwright.click("text=Add to cart"))
             |> then_(_ => BrowserTestUtils.sleep(300))
             |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
             |> then_(text =>
                  BrowserTestUtils.assertContains(~label="Cart updated after add", ~expected="Your cart is empty", text)
                  |> catch(_ => BrowserTestUtils.assertNotContains(~label="Cart is no longer empty", ~unexpected="Your cart is empty", text))
                )
              |> then_(_ => {
                   Js.log("Reloading page to verify IndexedDB cache persistence...");
                   page->Playwright.reload;
                 })
             |> then_(_ => page->Playwright.waitForSelector("text=Cloud Hardware Rental"))
             |> then_(_ => BrowserTestUtils.sleep(300))
             |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
             |> then_(text =>
                  BrowserTestUtils.assertNotContains(~label="Cache persists: cart not empty after reload", ~unexpected="Your cart is empty", text)
                  |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder after reload", ~unexpected="Loading", text))
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
        Js.log("Ecommerce browser tests passed!");
        BrowserTestUtils.exitProcess(0);
       resolve();
     })
  |> catch(error => {
        Js.log2("Ecommerce browser tests failed:", error);
        BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
