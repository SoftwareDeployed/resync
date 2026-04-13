open Lwt.Syntax

let suite =
  ( "action_store",
    [
      Alcotest.test_case "in_memory guard allows first call" `Quick (fun () ->
          let m = (Obj.magic () : (module Caqti_lwt.CONNECTION)) in
          let called = ref false in
          let callback () =
            called := true;
            Lwt.return (Mutation_result.Ack (Ok ()))
          in
          let result =
            Lwt_main.run
              (In_memory_action_store.with_guard m ~mutation_name:"m1" ~action_id:"a1"
                 callback)
          in
          match result with
          | Mutation_result.Ack (Ok ()) when !called -> ()
          | _ -> Alcotest.fail "Expected callback to run and return Ok");
      Alcotest.test_case "in_memory guard blocks duplicate ok" `Quick (fun () ->
          let m = (Obj.magic () : (module Caqti_lwt.CONNECTION)) in
          let called = ref 0 in
          let callback () =
            incr called;
            Lwt.return (Mutation_result.Ack (Ok ()))
          in
          let _ =
            Lwt_main.run
              (In_memory_action_store.with_guard m ~mutation_name:"m1" ~action_id:"a2"
                 callback)
          in
          let result =
            Lwt_main.run
              (In_memory_action_store.with_guard m ~mutation_name:"m1" ~action_id:"a2"
                 callback)
          in
          match result with
          | Mutation_result.Ack (Ok ()) when !called = 1 -> ()
          | _ -> Alcotest.fail "Expected duplicate to return Ok without running callback");
      Alcotest.test_case "in_memory guard records failure" `Quick (fun () ->
          let m = (Obj.magic () : (module Caqti_lwt.CONNECTION)) in
          let result =
            Lwt_main.run
              (In_memory_action_store.with_guard m ~mutation_name:"m1" ~action_id:"a3"
                 (fun () -> Lwt.return (Mutation_result.Ack (Error "bad"))))
          in
          let result2 =
            Lwt_main.run
              (In_memory_action_store.with_guard m ~mutation_name:"m1" ~action_id:"a3"
                 (fun () -> Lwt.return (Mutation_result.Ack (Ok ()))))
          in
          match result, result2 with
          | Mutation_result.Ack (Error "bad"), Mutation_result.Ack (Error "bad") -> ()
          | _ -> Alcotest.fail "Expected failure to be recorded and replayed");
      Alcotest.test_case "in_memory guard NoAck leaves no entry" `Quick (fun () ->
          let m = (Obj.magic () : (module Caqti_lwt.CONNECTION)) in
          let called = ref 0 in
          let callback () =
            incr called;
            Lwt.return Mutation_result.NoAck
          in
          let _ =
            Lwt_main.run
              (In_memory_action_store.with_guard m ~mutation_name:"m1" ~action_id:"a4"
                 callback)
          in
          let result =
            Lwt_main.run
              (In_memory_action_store.with_guard m ~mutation_name:"m1" ~action_id:"a4"
                 callback)
          in
          match result with
          | Mutation_result.NoAck when !called = 2 -> ()
          | _ -> Alcotest.fail "Expected NoAck to not record entry");
      Alcotest.test_case "record_failed marks failed" `Quick (fun () ->
          let m = (Obj.magic () : (module Caqti_lwt.CONNECTION)) in
          let result =
            Lwt_main.run
              (In_memory_action_store.record_failed m ~mutation_name:"m1"
                 ~action_id:"a5" ~msg:"oops")
          in
          match result with
          | Ok () -> ()
          | Error _ -> Alcotest.fail "Expected record_failed to succeed");
    ] )
