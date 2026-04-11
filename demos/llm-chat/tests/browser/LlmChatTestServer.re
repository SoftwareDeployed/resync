let defaultPort = 9998;

let baseUrl = "http://127.0.0.1:" ++ Js.Int.toString(defaultPort);

let start = () =>
  TestServer.buildAndStart(
    ~buildArgs=[|
      "build",
      "demos/llm-chat/ui/src/.build_stamp",
      "demos/llm-chat/server/src/server.exe",
    |],
     ~spawnArgs=[|
       "LLM_CHAT_DOC_ROOT=./_build/default/demos/llm-chat/ui/src",
        "SERVER_PORT=" ++ Js.Int.toString(defaultPort),
       "SERVER_INTERFACE=127.0.0.1",
       "DB_URL=postgres://executor:executor-password@localhost:5432/executor_db",
       "./_build/default/demos/llm-chat/server/src/server.exe",
     |],
    ~baseUrl,
    ~timeoutLabel="llm-chat browser test server",
    (),
  );

let stop = TestServer.stop;
