open StreamPipe

let suite =
  ( "StreamPipe",
    [
      Alcotest.test_case "make + subscribe receives values" `Quick (fun () ->
          let received = ref [||] in
          let pipe = make ~subscribe:(fun next ->
            next 1;
            next 2;
            next 3;
            fun () -> ()
          ) in
          let _unsubscribe = subscribe pipe (fun a ->
            received := Js.Array.concat ~other:[|a|] !received
          ) in
          if Array.length !received = 3
             && Array.get !received 0 = 1
             && Array.get !received 1 = 2
             && Array.get !received 2 = 3
          then ()
          else Alcotest.fail "Expected [1, 2, 3]");
      Alcotest.test_case "map doubles each value" `Quick (fun () ->
          let received = ref [||] in
          let pipe = map
            (make ~subscribe:(fun next ->
              next 1;
              next 2;
              next 3;
              fun () -> ()
            ))
            (fun a -> a * 2)
          in
          let _unsubscribe = subscribe pipe (fun a ->
            received := Js.Array.concat ~other:[|a|] !received
          ) in
          if Array.length !received = 3
             && Array.get !received 0 = 2
             && Array.get !received 1 = 4
             && Array.get !received 2 = 6
          then ()
          else Alcotest.fail "Expected [2, 4, 6]");
      Alcotest.test_case "filterMap keeps only even numbers and halves them" `Quick (fun () ->
          let received = ref [||] in
          let pipe = filterMap
            (make ~subscribe:(fun next ->
              next 1;
              next 2;
              next 3;
              next 4;
              fun () -> ()
            ))
            (fun a ->
              if a mod 2 = 0 then
                Some (a / 2)
              else
                None)
          in
          let _unsubscribe = subscribe pipe (fun a ->
            received := Js.Array.concat ~other:[|a|] !received
          ) in
          if Array.length !received = 2
             && Array.get !received 0 = 1
             && Array.get !received 1 = 2
          then ()
          else Alcotest.fail "Expected [1, 2]");
      Alcotest.test_case "tap runs side effect without changing values" `Quick (fun () ->
          let sideEffect = ref [||] in
          let received = ref [||] in
          let pipe = tap
            (make ~subscribe:(fun next ->
              next 1;
              next 2;
              fun () -> ()
            ))
            (fun a ->
              sideEffect := Js.Array.concat ~other:[|a|] !sideEffect)
          in
          let _unsubscribe = subscribe pipe (fun a ->
            received := Js.Array.concat ~other:[|a|] !received
          ) in
          if Array.length !received = 2
             && Array.get !received 0 = 1
             && Array.get !received 1 = 2
             && Array.length !sideEffect = 2
             && Array.get !sideEffect 0 = 1
             && Array.get !sideEffect 1 = 2
          then ()
          else Alcotest.fail "tap did not work correctly");
      Alcotest.test_case "nested map + filterMap compose correctly" `Quick (fun () ->
          let received = ref [||] in
          let pipe = filterMap
            (map
              (make ~subscribe:(fun next ->
                next 1;
                next 2;
                next 3;
                next 4;
                fun () -> ()
              ))
              (fun a -> a * 3))
            (fun a ->
              if a mod 2 = 0 then
                Some (a / 2)
              else
                None)
          in
          let _unsubscribe = subscribe pipe (fun a ->
            received := Js.Array.concat ~other:[|a|] !received
          ) in
          (* 1*3=3 (odd, dropped), 2*3=6 -> 3, 3*3=9 (odd, dropped), 4*3=12 -> 6 *)
          if Array.length !received = 2
             && Array.get !received 0 = 3
             && Array.get !received 1 = 6
          then ()
          else Alcotest.fail "Expected [3, 6]");
    ] )
