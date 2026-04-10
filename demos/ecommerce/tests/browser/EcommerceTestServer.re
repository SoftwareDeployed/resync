open Js.Promise;

type response;
type childProcess;
type spawnOptions;

type t = {
  baseUrl: string,
  process: childProcess,
};

let defaultPort = 9888;

let baseUrl = "http://127.0.0.1:" ++ Js.Int.toString(defaultPort);

[@mel.module "node:child_process"]
external spawn:
  (string, array(string), spawnOptions) => childProcess = "spawn";

[@mel.module "node:child_process"]
external execFileSync:
  (string, array(string), spawnOptions) => unit = "execFileSync";

[@mel.obj]
external makeSpawnOptions:
  (~cwd: string, ~stdio: string, unit) => spawnOptions = "";

[@mel.scope "process"]
external cwd: unit => string = "cwd";

[@mel.scope "process"]
external env: Js.Dict.t(string) = "env";

external fetchUrl: string => Js.Promise.t(response) = "fetch";

[@mel.get]
external responseOk: response => bool = "ok";

[@mel.send]
external kill: (childProcess, string) => bool = "kill";

[@mel.module "node:timers/promises"]
external sleep: int => Js.Promise.t(unit) = "setTimeout";

let rec waitForReady = attemptsLeft =>
  if (attemptsLeft <= 0) {
    reject(Failure("Timed out waiting for ecommerce browser test server"));
  } else {
    fetchUrl(baseUrl)
    |> then_(response =>
         if (responseOk(response)) {
           resolve();
         } else {
           sleep(200) |> then_(_ => waitForReady(attemptsLeft - 1));
         }
       )
    |> catch(_error => sleep(200) |> then_(_ => waitForReady(attemptsLeft - 1)));
  };

let getDbUrl = () => {
  switch (Js.Dict.get(env, "DB_URL")) {
  | Some(url) => url
  | None => "postgres://executor:executor-password@localhost:5432/executor_db"
  };
};

let start = () => {
  try {
    execFileSync(
      "dune",
      [|"build", "demos/ecommerce/ui/src/.build_stamp", "demos/ecommerce/server/src/server.exe"|],
      makeSpawnOptions(~cwd=cwd(), ~stdio="inherit", ()),
    );

  let process =
    spawn(
      "env",
      [|
        "ECOMMERCE_DOC_ROOT=./_build/default/demos/ecommerce/ui/src",
        "API_BASE_URL=" ++ baseUrl,
        "DB_URL=" ++ getDbUrl(),
        "SERVER_INTERFACE=127.0.0.1",
        "SERVER_PORT=" ++ Js.Int.toString(defaultPort),
        "./_build/default/demos/ecommerce/server/src/server.exe",
      |],
      makeSpawnOptions(~cwd=cwd(), ~stdio="inherit", ()),
    );

  waitForReady(100)
  |> then_(_ => resolve({baseUrl, process}));
  } {
  | _ => reject(Failure("Failed to build ecommerce browser test server artifacts"))
  };
};

let stop = (server: t) => {
  let _ = kill(server.process, "SIGTERM");
  sleep(200);
};
