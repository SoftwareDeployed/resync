open Js.Promise;

type response;
type childProcess;
type spawnOptions;

type t = {
  baseUrl: string,
  process: childProcess,
};

let defaultPort = 9999;

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

external fetchUrl: string => Js.Promise.t(response) = "fetch";

[@mel.get]
external responseOk: response => bool = "ok";

[@mel.send]
external kill: (childProcess, string) => bool = "kill";

[@mel.module "node:timers/promises"]
external sleep: int => Js.Promise.t(unit) = "setTimeout";

let rec waitForReady = attemptsLeft =>
  if (attemptsLeft <= 0) {
    reject(Failure("Timed out waiting for video-chat browser test server"));
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

let start = () => {
  try {
    execFileSync(
      "dune",
      [|"build", "demos/video-chat/ui/src/.build_stamp", "demos/video-chat/server/src/server.exe"|],
      makeSpawnOptions(~cwd=cwd(), ~stdio="inherit", ()),
    );

  let process =
    spawn(
      "env",
      [|
        "VIDEO_CHAT_DOC_ROOT=./_build/default/demos/video-chat/ui/src",
        "VIDEO_CHAT_SERVER_PORT=" ++ Js.Int.toString(defaultPort),
        "SERVER_INTERFACE=127.0.0.1",
        "./_build/default/demos/video-chat/server/src/server.exe",
      |],
      makeSpawnOptions(~cwd=cwd(), ~stdio="inherit", ()),
    );

  waitForReady(100)
  |> then_(_ => resolve({baseUrl, process}));
  } {
  | _ => reject(Failure("Failed to build video-chat browser test server artifacts"))
  };
};

let stop = (server: t) => {
  let _ = kill(server.process, "SIGTERM");
  sleep(200);
};
