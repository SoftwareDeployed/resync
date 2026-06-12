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
      Alcotest.test_case "select requests keep distinct logical subscriptions" `Quick
        (fun () ->
          let requests =
            RealtimeClient.uniqueSelectRequests
              [| ("thread:room-1", 1.0); ("messages:room-1", 2.0); ("thread:room-1", 3.0) |]
          in
          Alcotest.(check int) "request count" 2 (Array.length requests);
          let first_subscription, first_updated_at = requests.(0) in
          let second_subscription, second_updated_at = requests.(1) in
          Alcotest.(check string)
            "first subscription"
            "thread:room-1"
            first_subscription;
          Alcotest.(check (float 0.0)) "first updated_at" 1.0 first_updated_at;
          Alcotest.(check string)
            "second subscription"
            "messages:room-1"
            second_subscription;
          Alcotest.(check (float 0.0)) "second updated_at" 2.0 second_updated_at);
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
