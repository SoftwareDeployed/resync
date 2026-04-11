let () =
  SseParser_suite.init ();
  StreamPipe_suite.init ();
  NdjsonParser_suite.init ();
  Test_framework.run_all () |> exit
