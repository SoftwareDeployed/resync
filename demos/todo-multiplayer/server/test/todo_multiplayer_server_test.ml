let () =
  Server_http_test.init ();
  Test_framework.run_all () |> exit
