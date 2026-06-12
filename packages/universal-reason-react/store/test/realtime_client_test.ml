let json_string value = Melange_json.Primitives.string_to_json value

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
      Alcotest.test_case "pending mutation queue dedupes by action id" `Quick
        (fun () ->
          let pending =
            RealtimeClientMultiplexed.enqueuePendingMutation
              ~actionId:"a"
              ~action:(json_string "first")
              [||]
          in
          let pending =
            RealtimeClientMultiplexed.enqueuePendingMutation
              ~actionId:"a"
              ~action:(json_string "second")
              pending
          in
          Alcotest.(check int) "queued count" 1 (Array.length pending);
          let action_id, _ = pending.(0) in
          Alcotest.(check string) "kept action id" "a" action_id);
      Alcotest.test_case "pending mutation queue removes sent action id" `Quick
        (fun () ->
          let pending =
            [| ("a", json_string "first"); ("b", json_string "second") |]
          in
          let remaining =
            RealtimeClientMultiplexed.removePendingMutation
              ~actionId:"a"
              pending
          in
          Alcotest.(check int) "remaining count" 1 (Array.length remaining);
          let action_id, _ = remaining.(0) in
          Alcotest.(check string) "remaining action id" "b" action_id);
    ] )
