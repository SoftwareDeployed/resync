let defaultPort = 9876;

let baseUrl = "http://127.0.0.1:" ++ Js.Int.toString(defaultPort);

let browserBuildDir = "_build_browser_tests/default";

let getDbUrl = () => {
  switch (Js.Dict.get(TestServer.env, "DB_URL")) {
  | Some(url) => url
  | None => "postgres://executor:executor-password@localhost:5432/executor_db"
  };
};

let start = () =>
  TestServer.buildAndStart(
    ~buildArgs=[|
      "build",
      "--build-dir",
      "_build_browser_tests",
      "demos/todo-multiplayer/ui/src/.build_stamp",
      "demos/todo-multiplayer/server/src/server.exe",
    |],
    ~spawnArgs=[|
      "TODO_MP_DOC_ROOT=./"
      ++ browserBuildDir
      ++ "/demos/todo-multiplayer/ui/src",
      "DB_URL=" ++ getDbUrl(),
      "SERVER_INTERFACE=127.0.0.1",
      "SERVER_PORT=" ++ Js.Int.toString(defaultPort),
      "./"
      ++ browserBuildDir
      ++ "/demos/todo-multiplayer/server/src/server.exe",
    |],
    ~baseUrl,
    ~timeoutLabel="todo-multiplayer browser test server",
    (),
  );

let stop = TestServer.stop;
