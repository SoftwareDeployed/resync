open Lwt.Syntax

module T = Caqti_type
open Mutation_result

let table_name mutation_name = "_resync_actions_" ^ mutation_name

let truncate_msg msg =
  if String.length msg > 4096 then String.sub msg 0 4096 else msg

type check_result =
  [ `Already_ok
  | `Already_failed of string
  | `New
  ]

let check (module Db : Caqti_lwt.CONNECTION) ~mutation_name ~action_id =
  let query =
    Caqti_request.Infix.(
      T.string ->? T.(t2 string (option string))
    ) (Printf.sprintf "SELECT status, error_message FROM %s WHERE action_id = $1" (table_name mutation_name))
  in
  let* result = Db.find_opt query action_id in
  match result with
  | Error err ->
    Printf.eprintf "[sql_action_store] check failed for %s.%s: %s\n%!" mutation_name action_id (Caqti_error.show err);
    Lwt.return `New
  | Ok None -> Lwt.return `New
  | Ok (Some ("ok", _)) -> Lwt.return `Already_ok
  | Ok (Some ("failed", Some msg)) -> Lwt.return (`Already_failed msg)
  | Ok (Some ("failed", None)) -> Lwt.return (`Already_failed "")
  | Ok (Some (_, _)) -> Lwt.return `New

let record_status (module Db : Caqti_lwt.CONNECTION) ~mutation_name ~action_id ~status ~error_message =
  let query =
    Caqti_request.Infix.(
      T.(t3 string string (option string)) ->. T.unit
    ) (Printf.sprintf "INSERT INTO %s (action_id, status, error_message) VALUES ($1, $2, $3)" (table_name mutation_name))
  in
  Db.exec query (action_id, status, error_message)

let record_failed (module Db : Caqti_lwt.CONNECTION) ~mutation_name ~action_id ~msg =
  let truncated = truncate_msg msg in
  Printf.eprintf "[sql_action_store] record_failed for %s.%s: %s\n%!" mutation_name action_id msg;
  record_status (module Db) ~mutation_name ~action_id ~status:"failed" ~error_message:(Some truncated)

let with_guard (module Db : Caqti_lwt.CONNECTION) ~mutation_name ~action_id callback =
  let db_module = (module Db : Caqti_lwt.CONNECTION) in
  let* check_result = check db_module ~mutation_name ~action_id in
  match check_result with
  | `Already_ok -> Lwt.return (Ack (Ok ()))
  | `Already_failed msg -> Lwt.return (Ack (Error msg))
  | `New ->
    let* start_result = Db.start () in
    match start_result with
    | Error err -> Lwt.return (Ack (Error (Caqti_error.show err)))
    | Ok () ->
      Lwt.catch
        (fun () ->
          let* result = callback () in
          match result with
          | Ack (Ok ()) ->
            let* record_result = record_status db_module ~mutation_name ~action_id ~status:"ok" ~error_message:None in
            (match record_result with
             | Error err -> Lwt.return (Ack (Error (Caqti_error.show err)))
             | Ok () ->
               let* commit_result = Db.commit () in
               (match commit_result with
                | Error err -> Lwt.return (Ack (Error (Caqti_error.show err)))
                | Ok () -> Lwt.return (Ack (Ok ()))))
          | Ack (Error msg) ->
            let* rollback_result = Db.rollback () in
            (match rollback_result with
             | Error err -> Lwt.return (Ack (Error (Caqti_error.show err)))
             | Ok () ->
               let* record_result = record_status db_module ~mutation_name ~action_id ~status:"failed" ~error_message:(Some (truncate_msg msg)) in
               (match record_result with
                | Error err -> Lwt.return (Ack (Error (Caqti_error.show err)))
                | Ok () -> Lwt.return (Ack (Error msg))))
          | NoAck ->
            let* rollback_result = Db.rollback () in
            (match rollback_result with
             | Error err -> Lwt.return (Ack (Error (Caqti_error.show err)))
             | Ok () -> Lwt.return NoAck))
        (fun exn ->
          let* _ = Db.rollback () in
          Lwt.fail exn)

include (struct
  let with_guard = with_guard
  let record_failed = record_failed
end : Action_store.S)
