open Lwt.Syntax

let db_uri () =
  match Sys.getenv_opt "DB_URL" with
  | Some value -> value
  | None -> failwith "DB_URL is required for pgnotify adapter tests"

let make_sender_connection () =
  let host, port, user, password, dbname = Pgnotify_adapter.parse_uri (db_uri ()) in
  new Postgresql.connection ~host ~port ~user ~password ~dbname ()

let safe_finish conn =
  try conn#finish with _ -> ()

let wait_for_message received_ref =
  let rec loop remaining =
    match !received_ref with
    | Some message -> Lwt.return_some message
    | None when remaining <= 0 -> Lwt.return_none
    | None ->
        let* () = Lwt_unix.sleep 0.1 in
        loop (remaining - 1)
  in
  loop 20

let suite =
  ( "pgnotify adapter",
    [
      Alcotest.test_case "subscribed handler receives patch notification" `Quick (fun () ->
          let adapter = Pgnotify_adapter.create ~db_uri:(db_uri ()) () in
          let channel = "resync_test_patch" in
          let received = ref None in
          let handler ?wrap:_ message =
            received := Some message;
            Lwt.return_unit
          in
          let result =
            Lwt_main.run
              (let* () = Pgnotify_adapter.start adapter in
               let* () = Pgnotify_adapter.subscribe adapter ~channel ~handler in
               let sender = make_sender_connection () in
               let _notify_result =
                 try
                   let result =
                     sender#exec
                       (Printf.sprintf
                          "NOTIFY \"%s\", '%s'"
                          channel
                          "{\"type\":\"patch\",\"payload\":{\"hello\":\"world\"}}")
                   in
                   safe_finish sender;
                   result
                 with exn ->
                   safe_finish sender;
                   raise exn
               in
               let* result = wait_for_message received in
               let* () = Pgnotify_adapter.unsubscribe adapter ~channel in
               let* () = Pgnotify_adapter.stop adapter in
               Lwt.return result)
          in
          match result with
          | Some message when String.contains message 'p' -> ()
          | Some message -> Alcotest.fail ("Unexpected notification payload: " ^ message)
          | None -> Alcotest.fail "Expected notification to be delivered");
      Alcotest.test_case "unsubscribe stops later delivery" `Quick (fun () ->
          let adapter = Pgnotify_adapter.create ~db_uri:(db_uri ()) () in
          let channel = "resync_test_unsub" in
          let received = ref None in
          let handler ?wrap:_ message =
            received := Some message;
            Lwt.return_unit
          in
          let result =
            Lwt_main.run
              (let* () = Pgnotify_adapter.start adapter in
               let* () = Pgnotify_adapter.subscribe adapter ~channel ~handler in
               let* () = Pgnotify_adapter.unsubscribe adapter ~channel in
               let sender = make_sender_connection () in
               let _notify_result =
                 try
                   let result =
                     sender#exec
                       (Printf.sprintf
                          "NOTIFY \"%s\", '%s'"
                          channel
                          "{\"type\":\"patch\",\"payload\":{\"ignored\":true}}")
                   in
                   safe_finish sender;
                   result
                 with exn ->
                   safe_finish sender;
                   raise exn
               in
               let* () = Lwt_unix.sleep 0.3 in
               let* () = Pgnotify_adapter.stop adapter in
               Lwt.return !received)
          in
          match result with
          | None -> ()
          | Some message -> Alcotest.fail ("Unexpected delivery after unsubscribe: " ^ message));
    ] )
