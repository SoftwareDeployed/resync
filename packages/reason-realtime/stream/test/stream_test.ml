let () =
  Alcotest.run "stream"
    [ SseParser_suite.suite; NdjsonParser_suite.suite; StreamPipe_suite.suite ]
