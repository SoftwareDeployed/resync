let defaultPort = 9888;

let baseUrl = "http://127.0.0.1:" ++ Js.Int.toString(defaultPort);

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
      "demos/ecommerce/ui/src/.build_stamp",
      "demos/ecommerce/server/src/server.exe",
    |],
    ~spawnArgs=[|
      "ECOMMERCE_DOC_ROOT=./_build/default/demos/ecommerce/ui/src",
      "API_BASE_URL=" ++ baseUrl,
      "DB_URL=" ++ getDbUrl(),
      "SERVER_INTERFACE=127.0.0.1",
      "SERVER_PORT=" ++ Js.Int.toString(defaultPort),
      "./_build/default/demos/ecommerce/server/src/server.exe",
    |],
    ~baseUrl,
    ~timeoutLabel="ecommerce browser test server",
    (),
  );

let stop = TestServer.stop;
