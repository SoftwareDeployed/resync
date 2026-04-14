type browserType;
type browser;
type page;
type browserContext;

type launchOptions;

[@mel.obj]
external makeLaunchOptions: (~headless: bool=?, unit) => launchOptions = "";

[@mel.module "@playwright/test"]
external chromium: browserType = "chromium";

[@mel.send]
external launch: (browserType, launchOptions) => Js.Promise.t(browser) = "launch";

[@mel.send]
external newPage: browser => Js.Promise.t(page) = "newPage";

[@mel.send]
external newContext: browser => Js.Promise.t(browserContext) = "newContext";

[@mel.send]
external newPageInContext: browserContext => Js.Promise.t(page) = "newPage";

[@mel.send]
external closeContext: browserContext => Js.Promise.t(unit) = "close";

[@mel.send]
external goto: (page, string) => Js.Promise.t(Js.Nullable.t('a)) = "goto";

[@mel.send]
external title: page => Js.Promise.t(string) = "title";

[@mel.send]
external fill: (page, string, string) => Js.Promise.t(unit) = "fill";

[@mel.send]
external click: (page, string) => Js.Promise.t(unit) = "click";

[@mel.send]
external check: (page, string) => Js.Promise.t(unit) = "check";

[@mel.send]
external textContent: (page, string) => Js.Promise.t(Js.Nullable.t(string)) = "textContent";

[@mel.send]
external waitForSelector: (page, string) => Js.Promise.t(Js.Nullable.t('a)) = "waitForSelector";

[@mel.send]
external waitForSelectorWithOptions: (page, string, Js.t({..})) => Js.Promise.t(Js.Nullable.t('a)) = "waitForSelector";

let waitForSelectorWithTimeout = (page, selector, ~timeout) =>
  waitForSelectorWithOptions(page, selector, {"timeout": timeout});

[@mel.send]
external reload: page => Js.Promise.t(Js.Nullable.t('a)) = "reload";

[@mel.send]
external close: browser => Js.Promise.t(unit) = "close";

[@mel.send]
external evaluateString: (page, string) => Js.Promise.t(string) = "evaluate";

[@mel.send]
external addInitScript: (page, string) => Js.Promise.t(unit) = "addInitScript";

[@mel.scope "process"]
external cwd: unit => string = "cwd";

/* Console message capture */
type consoleMessage;

[@mel.send]
external text: consoleMessage => string = "text";

[@mel.send]
external type_: consoleMessage => string = "type";

[@mel.send]
external onConsole: (page, string, consoleMessage => unit) => unit = "on";

[@mel.send]
external onPageError: (page, string, string => unit) => unit = "on";
