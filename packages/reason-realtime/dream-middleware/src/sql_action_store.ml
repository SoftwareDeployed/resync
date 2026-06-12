open Lwt.Syntax

module T = Caqti_type
open Mutation_result

let table_name mutation_name = "_resync_actions_" ^ mutation_name

let truncate_msg msg =
  if String.length msg > 4096 then String.sub msg 0 4096 else msg

let local_error msg =
  Caqti_error.request_failed
    ~uri:(Uri.of_string "")
    ~query:"resync action ledger"
    (Caqti_error.Msg msg)

let best_effort_rollback (module Db : Caqti_lwt.CONNECTION) =
  Lwt.catch
    (fun () ->
       let* _ = Db.rollback () in
       Lwt.return_unit)
    (fun _ -> Lwt.return_unit)

let return_error_after_rollback db error =
  let* () = best_effort_rollback db in
  Lwt.return (Error error)

let is_safe_identifier_suffix value =
  String.length value > 0
  &&
  String.for_all
    (function
      | 'a' .. 'z' | 'A' .. 'Z' | '0' .. '9' | '_' -> true
      | _ -> false)
    value

type storage =
  | Per_mutation_table of string
  | Generic_table

type check_result =
  [ `Already_ok
  | `Already_failed of string
  | `New
  ]

let table_exists (module Db : Caqti_lwt.CONNECTION) table =
  let query =
    Caqti_request.Infix.(
      T.string ->! T.string
    ) "SELECT COALESCE(to_regclass($1)::text, '')"
  in
  let* result = Db.find query table in
  match result with
  | Ok "" -> Lwt.return (Ok false)
  | Ok _ -> Lwt.return (Ok true)
  | Error err -> Lwt.return (Error err)

let resolve_storage db ~mutation_name =
  let per_mutation_table =
    if is_safe_identifier_suffix mutation_name then Some (table_name mutation_name) else None
  in
  let* per_mutation_exists =
    match per_mutation_table with
    | None -> Lwt.return (Ok false)
    | Some table -> table_exists db table
  in
  match per_mutation_table, per_mutation_exists with
  | Some table, Ok true -> Lwt.return (Ok (Per_mutation_table table))
  | None, Ok true -> Lwt.return (Ok Generic_table)
  | _, Ok false ->
    let* generic_exists = table_exists db "_resync_actions" in
    (match generic_exists with
     | Ok true -> Lwt.return (Ok Generic_table)
     | Ok false ->
       Lwt.return
         (Error
            (`Msg
               (Printf.sprintf
                  "No action ledger table found for mutation %S. Expected %s or _resync_actions."
                  mutation_name
                  (match per_mutation_table with Some table -> table | None -> "<invalid mutation table name>"))))
     | Error err -> Lwt.return (Error (`Caqti err)))
  | _, Error err -> Lwt.return (Error (`Caqti err))

let check (module Db : Caqti_lwt.CONNECTION) storage ~mutation_name ~action_id =
  let* result =
    match storage with
    | Per_mutation_table table ->
      let query =
        Caqti_request.Infix.(
          T.string ->? T.(t2 string (option string))
        ) (Printf.sprintf "SELECT status, error_message FROM %s WHERE action_id = $1" table)
      in
      Db.find_opt query action_id
    | Generic_table ->
      let query =
        Caqti_request.Infix.(
          T.(t2 string string) ->? T.(t2 string (option string))
        ) "SELECT status, error_message FROM _resync_actions WHERE action_id = $1 AND mutation_name = $2"
      in
      Db.find_opt query (action_id, mutation_name)
  in
  match result with
  | Error err ->
    Printf.eprintf "[sql_action_store] check failed for %s.%s: %s\n%!" mutation_name action_id (Caqti_error.show err);
    Lwt.return (Error err)
  | Ok None -> Lwt.return (Ok `New)
  | Ok (Some ("ok", _)) -> Lwt.return (Ok `Already_ok)
  | Ok (Some ("failed", Some msg)) -> Lwt.return (Ok (`Already_failed msg))
  | Ok (Some ("failed", None)) -> Lwt.return (Ok (`Already_failed ""))
  | Ok (Some (_, _)) -> Lwt.return (Ok `New)

let record_status (module Db : Caqti_lwt.CONNECTION) storage ~mutation_name ~action_id ~status ~error_message =
  match storage with
  | Per_mutation_table table ->
      Caqti_request.Infix.(
        T.(t3 string string (option string)) ->. T.unit
      ) (Printf.sprintf "INSERT INTO %s (action_id, status, error_message) VALUES ($1, $2, $3)" table)
      |> fun query -> Db.exec query (action_id, status, error_message)
  | Generic_table ->
      Caqti_request.Infix.(
        T.(t4 string string string (option string)) ->. T.unit
      ) "INSERT INTO _resync_actions (action_id, mutation_name, status, error_message) VALUES ($1, $2, $3, $4)"
      |> fun query -> Db.exec query (action_id, mutation_name, status, error_message)

let record_failed (module Db : Caqti_lwt.CONNECTION) ~mutation_name ~action_id ~msg =
  let truncated = truncate_msg msg in
  Printf.eprintf "[sql_action_store] record_failed for %s.%s: %s\n%!" mutation_name action_id msg;
  let db_module = (module Db : Caqti_lwt.CONNECTION) in
  let* storage = resolve_storage db_module ~mutation_name in
  match storage with
  | Ok storage ->
    let* result =
      record_status db_module storage ~mutation_name ~action_id ~status:"failed" ~error_message:(Some truncated)
    in
    (match result with
     | Ok () -> Lwt.return (Ok ())
     | Error err -> return_error_after_rollback db_module err)
  | Error (`Caqti err) -> return_error_after_rollback db_module err
  | Error (`Msg msg) -> Lwt.return (Error (local_error msg))

let with_guard (module Db : Caqti_lwt.CONNECTION) ~mutation_name ~action_id callback =
  let db_module = (module Db : Caqti_lwt.CONNECTION) in
  let ack_error message = Lwt.return (Ack (Error message)) in
  let ack_caqti_error err =
    let* () = best_effort_rollback db_module in
    ack_error (Caqti_error.show err)
  in
  let* storage = resolve_storage db_module ~mutation_name in
  match storage with
  | Error (`Caqti err) -> ack_caqti_error err
  | Error (`Msg msg) -> ack_error msg
  | Ok storage ->
    let* check_result = check db_module storage ~mutation_name ~action_id in
    match check_result with
    | Error err -> ack_caqti_error err
    | Ok `Already_ok -> Lwt.return (Ack (Ok ()))
    | Ok (`Already_failed msg) -> Lwt.return (Ack (Error msg))
    | Ok `New ->
      let* start_result = Db.start () in
      match start_result with
      | Error err -> ack_caqti_error err
      | Ok () ->
        Lwt.catch
          (fun () ->
            let* result = callback () in
            match result with
            | Ack (Ok ()) ->
              let* record_result = record_status db_module storage ~mutation_name ~action_id ~status:"ok" ~error_message:None in
              (match record_result with
               | Error err ->
                 let* _ = Db.rollback () in
                 Lwt.return (Ack (Error (Caqti_error.show err)))
               | Ok () ->
                 let* commit_result = Db.commit () in
                 (match commit_result with
                  | Error err -> ack_caqti_error err
                  | Ok () -> Lwt.return (Ack (Ok ()))))
            | Ack (Error msg) ->
              let* rollback_result = Db.rollback () in
              (match rollback_result with
               | Error err -> Lwt.return (Ack (Error (Caqti_error.show err)))
               | Ok () ->
                 let* record_result = record_status db_module storage ~mutation_name ~action_id ~status:"failed" ~error_message:(Some (truncate_msg msg)) in
                 (match record_result with
                  | Error err -> ack_caqti_error err
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
