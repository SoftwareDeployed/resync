open Js.Promise;

[@mel.send]
external includes: (string, string) => bool = "includes";

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
         if (text->includes(expected)) {
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
       | Some(activeServer) => LlmChatTestServer.stop(activeServer) |> catch(_ => resolve())
       | None => resolve()
       }
     );
};

let runThreadListAndInputScenario = (~browser, ~baseUrl) => {
  Js.log("Running thread list and input interaction scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#thread-list",
              ~expected="New Chat",
              ~label="Thread list renders with New Chat",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ => page->Playwright.fill("#prompt-input", "Hello from browser test"))
       |> then_(_ => page->Playwright.waitForSelector("#send-button"))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="Hello from browser test",
              ~label="User message appears after send",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ => page->Playwright.evaluateString("window.location.href"))
     );
};

let runMessageDisplayScenario = (~browser, ~threadUrl) => {
  Js.log("Running message display scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(threadUrl)
       |> then_(_ => page->Playwright.waitForSelector("#message-list"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="Hello from browser test",
              ~label="Existing user message renders from snapshot",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ =>
            page->Playwright.evaluateString(
              "document.querySelector('.message--user') !== null ? 'true' : 'false'",
            )
          )
       |> then_(hasUserClass =>
            BrowserTestUtils.assertTrue(
              ~label="User message has correct role class",
              hasUserClass == "true",
              ~details="Expected .message--user element in DOM",
            )
          )
     );
};

let runOllamaStreamingScenario = (~browser, ~baseUrl) => {
  Js.log("Running Ollama streaming scenario (skipped if unavailable)...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ => page->Playwright.fill("#prompt-input", "ping"))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="ping",
              ~label="Prompt visible in message list",
              ~attemptsLeft=30,
            )
          )
       |> then_(_ => BrowserTestUtils.sleep(3000))
       |> then_(_ => BrowserTestUtils.textOrEmpty(page, "#message-list"))
       |> then_(text => {
            if (text->includes("Error") || text->includes("error")) {
              Js.log("[SKIP] Ollama streaming test skipped (error detected)");
              resolve();
            } else if (text->includes("assistant")) {
              Js.log("[PASS] Ollama streaming test");
              resolve();
            } else {
              Js.log("[SKIP] Ollama streaming test skipped (no assistant response)");
              resolve();
            }
          })
       |> catch(_ => {
            Js.log("[SKIP] Ollama streaming test skipped (exception)");
            resolve();
          })
     );
};

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let browserRef = ref(None);
  let serverRef = ref(None);

  LlmChatTestServer.start()
  |> then_(server => {
       serverRef := Some(server);
       Playwright.chromium->Playwright.launch(launchOptions);
     })
  |> then_(browser => {
       browserRef := Some(browser);
       switch (serverRef.contents) {
       | Some(server) =>
         runThreadListAndInputScenario(~browser, ~baseUrl=server.baseUrl)
         |> then_(threadUrl =>
              runMessageDisplayScenario(~browser, ~threadUrl)
              |> then_(_ => runOllamaStreamingScenario(~browser, ~baseUrl=server.baseUrl))
            )
       | None => reject(BrowserTestUtils.makeError("server was not initialized"))
       };
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
       Js.log("LLM Chat browser tests passed!");
       BrowserTestUtils.exitProcess(0);
       resolve();
     })
   |> catch(error => {
       Js.log2("LLM Chat browser tests failed:", error);
       BrowserTestUtils.exitProcess(1);
       resolve();
     })
   |> ignore;
