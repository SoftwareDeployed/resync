let () =
  NdjsonParser_suite.init ();
  Test_framework.run_all () |> exit
