let () =
  SseParser_test.init ();
  StreamPipe_test.init ();
  NdjsonParser_test.init ();
  Test_framework.run_all () |> exit
