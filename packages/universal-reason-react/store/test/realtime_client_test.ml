let suite =
  ( "RealtimeClient",
    [
      Alcotest.test_case "channelIdOfSubscription strips one type prefix" `Quick
        (fun () ->
          Alcotest.(check string)
            "prefixed channel"
            "019ebaad-3fed-7b7b-b36f-453970002fae"
            (RealtimeClient.channelIdOfSubscription
               "thread:019ebaad-3fed-7b7b-b36f-453970002fae"));
      Alcotest.test_case
        "channelIdOfSubscription preserves colons after the prefix" `Quick
        (fun () ->
          Alcotest.(check string)
            "suffix"
            "tenant:room"
            (RealtimeClient.channelIdOfSubscription "scope:tenant:room"));
      Alcotest.test_case "channelIdOfSubscription keeps plain channels" `Quick
        (fun () ->
          Alcotest.(check string)
            "plain channel"
            "019ebaad-3fed-7b7b-b36f-453970002fae"
            (RealtimeClient.channelIdOfSubscription
               "019ebaad-3fed-7b7b-b36f-453970002fae"));
    ] )
