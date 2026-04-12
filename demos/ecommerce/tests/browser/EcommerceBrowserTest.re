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

let setInventoryQuantity = (page, ~baseUrl, ~itemId, ~quantity) => {
  let url = baseUrl ++ "/api/test/inventory/" ++ itemId ++ "/quantity/" ++ Int.to_string(quantity);
  page->Playwright.goto(url);
};

let updateQuantityViaFetch = (page, ~baseUrl, ~itemId, ~quantity) => {
  let url = baseUrl ++ "/api/test/inventory/" ++ itemId ++ "/quantity/" ++ Int.to_string(quantity);
  let jsCode =
    "fetch('" ++ url ++ "').then(function(r) { return r.text(); })";
  page->Playwright.evaluateString(jsCode);
};

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let browserRef = ref(None);
  let serverRef: ref(option(TestServer.t)) = ref(None);
  let itemId = "b55351b1-1b78-4b6c-bd13-6859dc9ad411";

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
       |> then_(page => {
            /* Reset item quantity to 1 for hermetic tests */
            setInventoryQuantity(page, ~baseUrl, ~itemId, ~quantity=1)
            |> then_(_ => page->Playwright.waitForSelector("text=updated"))
            |> then_(_ => {
                 Js.log("Reset inventory quantity to 1");
                 page->Playwright.goto(baseUrl ++ "/");
               })
            |> then_(_ => page->Playwright.waitForSelector("text=Cloud Hardware Rental"))
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
            |> then_(text =>
                 BrowserTestUtils.assertContains(~label="SSR heading visible", ~expected="Cloud Hardware Rental", text)
                 |> then_(_ => BrowserTestUtils.assertContains(~label="SSR cart visible", ~expected="Selected equipment", text))
                 |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder on initial load", ~unexpected="Loading", text))
               )
            |> then_(_ => page->Playwright.waitForSelector("text=Add to cart"))
            |> then_(_ => page->Playwright.click("text=Add to cart"))
            |> then_(_ => BrowserTestUtils.waitForIDBContent(page, ~dbName="ecommerce.cart", ~expectedText="inventory_id"))
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
            |> then_(text =>
                 BrowserTestUtils.assertNotContains(~label="Cart not empty after add", ~unexpected="Your cart is empty", text)
               )
            |> then_(_ => {
                 Js.log("Reloading page to verify IndexedDB cache persistence...");
                 page->Playwright.reload;
               })
            |> then_(_ => page->Playwright.waitForSelector("text=Cloud Hardware Rental"))
            |> then_(_ => BrowserTestUtils.waitForIDBContent(page, ~dbName="ecommerce.cart", ~expectedText="inventory_id"))
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
            |> then_(text =>
                 BrowserTestUtils.assertNotContains(~label="Cache persists: cart not empty after reload", ~unexpected="Your cart is empty", text)
                 |> then_(_ => BrowserTestUtils.assertNotContains(~label="No loading placeholder after reload", ~unexpected="Loading", text))
               )
            /* Empty cart scenario */
            |> then_(_ => {
                 Js.log("Testing empty cart behavior...");
                 page->Playwright.click("text=Selected equipment");
               })
            |> then_(_ => page->Playwright.waitForSelector("text=Remove"))
            |> then_(_ => page->Playwright.click("text=Remove"))
            |> then_(_ => BrowserTestUtils.waitForBodyContains(page, ~expectedText="Your cart is empty."))
            /* Quantity edge handling */
            |> then_(_ => {
                 Js.log("Testing quantity edge handling...");
                 page->Playwright.goto(baseUrl ++ "/");
               })
            |> then_(_ => page->Playwright.waitForSelector("text=Add to cart"))
            |> then_(_ => page->Playwright.click("text=Add to cart"))
            |> then_(_ => page->Playwright.click("text=Add to cart"))
            |> then_(_ => page->Playwright.click("text=Selected equipment"))
            |> then_(_ => page->Playwright.waitForSelector("text=Remove"))
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "body"))
            |> then_(text =>
                 BrowserTestUtils.assertContains(~label="Quantity capped at stock level", ~expected="Qty 1", text)
                 |> then_(_ => BrowserTestUtils.assertNotContains(~label="Quantity did not exceed stock", ~unexpected="Qty 2", text))
               )
            /* Realtime inventory update scenario */
            |> then_(_ => {
                 Js.log("Testing realtime inventory update...");
                 page->Playwright.goto(baseUrl ++ "/");
               })
            |> then_(_ => page->Playwright.waitForSelector("text=Add to cart"))
            |> then_(_ => page->Playwright.waitForSelector("#inventory-stock-" ++ itemId))
            |> then_(_ => BrowserTestUtils.textOrEmpty(page, "#inventory-stock-" ++ itemId))
            |> then_(stockBefore => {
                 BrowserTestUtils.assertContains(
                   ~label="Initial stock is 1 before realtime update",
                   ~expected="Stock: 1",
                   stockBefore,
                 )
                 |> then_(_ => {
                      Js.log("Updating inventory quantity to 2 via fetch...");
                      updateQuantityViaFetch(page, ~baseUrl, ~itemId, ~quantity=2);
                    })
                 |> then_(_ => BrowserTestUtils.sleep(500))
                 |> then_(_ => BrowserTestUtils.waitForBodyContains(page, ~expectedText="Stock: 2"))
                 |> then_(_ => {
                      Js.log("Updating inventory quantity back to 1 via fetch...");
                      updateQuantityViaFetch(page, ~baseUrl, ~itemId, ~quantity=1);
                    })
                 |> then_(_ => BrowserTestUtils.sleep(500))
                 |> then_(_ => BrowserTestUtils.waitForBodyContains(page, ~expectedText="Stock: 1"))
               })
          })
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
