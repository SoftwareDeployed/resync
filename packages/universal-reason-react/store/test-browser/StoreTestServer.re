let defaultPort = 8090;

let baseUrl = "http://127.0.0.1:" ++ Js.Int.toString(defaultPort);

let start = () =>
  TestServer.buildAndStart(
    ~buildArgs=[|
      "build",
      "demos/todo/ui/src/.build_stamp",
      "demos/todo/server/src/server.exe",
    |],
    ~spawnArgs=[|
      "TODO_DOC_ROOT=./_build/default/demos/todo/ui/src",
      "SERVER_INTERFACE=127.0.0.1",
      "SERVER_PORT=" ++ Js.Int.toString(defaultPort),
      "./_build/default/demos/todo/server/src/server.exe",
    |],
    ~baseUrl,
    ~timeoutLabel="store browser test server",
    (),
  );

let stop = TestServer.stop;
