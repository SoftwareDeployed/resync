open Js.Promise;

type response;
type childProcess;
type spawnOptions;

type t = {
  baseUrl: string,
  process: childProcess,
};

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

let rec waitForReady = (~url: string, ~attempts: int, ()) =>
  if (attempts <= 0) {
    reject(Failure("Timed out waiting for server at " ++ url));
  } else {
    fetchUrl(url)
    |> then_(response =>
         if (responseOk(response)) {
           resolve();
         } else {
           sleep(200) |> then_(_ => waitForReady(~url, ~attempts=attempts - 1, ()));
         }
       )
    |> catch(_error =>
         sleep(200) |> then_(_ => waitForReady(~url, ~attempts=attempts - 1, ()))
       );
  };

let buildAndStart = (~buildArgs: array(string), ~spawnArgs: array(string), ~baseUrl: string, ~timeoutLabel: string, ()) => {
  try {
    execFileSync(
      "dune",
      buildArgs,
      makeSpawnOptions(~cwd=cwd(), ~stdio="inherit", ()),
    );

    let process =
      spawn(
        "env",
        spawnArgs,
        makeSpawnOptions(~cwd=cwd(), ~stdio="inherit", ()),
      );

    waitForReady(~url=baseUrl, ~attempts=100, ())
    |> then_(_ => resolve({baseUrl, process}));
  } {
  | _ => reject(Failure("Failed to build " ++ timeoutLabel ++ " artifacts"))
  };
};

let stop = (server: t) => {
  let _ = kill(server.process, "SIGTERM");
  sleep(200);
};
