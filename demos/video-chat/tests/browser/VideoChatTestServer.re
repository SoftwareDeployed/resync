let defaultPort = 9999;

let baseUrl = "http://127.0.0.1:" ++ Js.Int.toString(defaultPort);

let start = () =>
  TestServer.buildAndStart(
    ~buildArgs=[|
      "build",
      "--build-dir",
      "_build_browser_tests",
      "demos/video-chat/ui/src/.build_stamp",
      "demos/video-chat/server/src/server.exe",
    |],
    ~spawnArgs=[|
      "VIDEO_CHAT_DOC_ROOT=./_build_browser_tests/default/demos/video-chat/ui/src",
      "VIDEO_CHAT_SERVER_PORT=" ++ Js.Int.toString(defaultPort),
      "SERVER_INTERFACE=127.0.0.1",
      "./_build_browser_tests/default/demos/video-chat/server/src/server.exe",
    |],
    ~baseUrl,
    ~timeoutLabel="video-chat browser test server",
    (),
  );

let stop = TestServer.stop;
