open Js.Promise;

[@mel.scope "process"]
external exitProcess: int => unit = "exit";

[@mel.send]
external includes: (string, string) => bool = "includes";

[@mel.new]
external makeError: string => exn = "Error";

let assertContains = (~label, ~expected, text) => {
  if (text->includes(expected)) {
    Js.log("[PASS] " ++ label);
    resolve();
  } else {
    reject(makeError(label ++ " missing expected text: " ++ expected));
  };
};

let assertEquals = (~label, ~expected, actual) => {
  if (actual == expected) {
    Js.log("[PASS] " ++ label);
    resolve();
  } else {
    reject(makeError(label ++ " expected " ++ expected ++ " but got " ++ actual));
  };
};

let bodyText = page =>
  page
  ->Playwright.textContent("body")
  |> then_(text =>
       resolve(
         switch (Js.Nullable.toOption(text)) {
         | Some(value) => value
         | None => ""
         },
       )
     );

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
            |> then_(_ => page->Playwright.title)
            |> then_(title => assertEquals(~label="Document title", ~expected="Todo App", title))
            |> then_(_ => page->Playwright.goto("http://127.0.0.1:8080/does-not-exist"))
            |> then_(_ => bodyText(page))
            |> then_(text => assertContains(~label="Deep link load", ~expected="My Todo List", text))
            |> then_(_ => browser->Playwright.close)
          )
     );
};

let () =
  run()
  |> then_(_ => {
       Js.log("Router browser tests passed!");
       exitProcess(0);
       resolve();
     })
  |> catch(error => {
       Js.log2("Router browser tests failed:", error);
       exitProcess(1);
       resolve();
     })
  |> ignore;
