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

let exec_unit (module Db : Caqti_lwt.CONNECTION) sql =
  let query =
    Caqti_request.Infix.(T.unit ->. T.unit) sql
  in
  Db.exec query ()

let exec_steps db sqls =
  let rec loop = function
    | [] -> Lwt.return (Ok ())
    | sql :: rest ->
        let* result = exec_unit db sql in
        (match result with
         | Ok () -> loop rest
         | Error err -> Lwt.return (Error err))
  in
  loop sqls

let is_safe_identifier_suffix value =
  String.length value > 0
  &&
  String.for_all
    (function
      | 'a' .. 'z' | 'A' .. 'Z' | '0' .. '9' | '_' -> true
      | _ -> false)
    value

type storage =
  | Per_mutation_table of {
      table : string;
      action_id_expr : string;
    }
  | Generic_table of {
      action_id_expr : string;
    }

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

let ensure_status_columns db table =
  let sql template = Printf.sprintf template table in
  exec_steps db
    [
      sql "ALTER TABLE %s ADD COLUMN IF NOT EXISTS status text DEFAULT 'ok'";
      sql "UPDATE %s SET status = 'ok' WHERE status IS NULL";
      sql "ALTER TABLE %s ALTER COLUMN status SET DEFAULT 'ok'";
      sql "ALTER TABLE %s ALTER COLUMN status SET NOT NULL";
      sql
        "ALTER TABLE %s ADD COLUMN IF NOT EXISTS processed_at timestamptz DEFAULT NOW()";
      sql "UPDATE %s SET processed_at = NOW() WHERE processed_at IS NULL";
      sql "ALTER TABLE %s ALTER COLUMN processed_at SET DEFAULT NOW()";
      sql "ALTER TABLE %s ALTER COLUMN processed_at SET NOT NULL";
      sql "ALTER TABLE %s ADD COLUMN IF NOT EXISTS error_message text";
    ]

let ensure_mutation_name_column db =
  exec_steps db
    [
      "ALTER TABLE _resync_actions ADD COLUMN IF NOT EXISTS mutation_name text DEFAULT ''";
      "UPDATE _resync_actions SET mutation_name = '' WHERE mutation_name IS NULL";
      "ALTER TABLE _resync_actions ALTER COLUMN mutation_name SET DEFAULT ''";
      "ALTER TABLE _resync_actions ALTER COLUMN mutation_name SET NOT NULL";
    ]

let ensure_generic_table db =
  let* created =
    exec_steps db
      [
        "CREATE TABLE IF NOT EXISTS _resync_actions (action_id text PRIMARY KEY, mutation_name text NOT NULL DEFAULT '', status text NOT NULL DEFAULT 'ok', processed_at timestamptz NOT NULL DEFAULT NOW(), error_message text)";
      ]
  in
  match created with
  | Error err -> Lwt.return (Error err)
  | Ok () ->
    let* mutation_name = ensure_mutation_name_column db in
    match mutation_name with
    | Error err -> Lwt.return (Error err)
    | Ok () -> ensure_status_columns db "_resync_actions"

let ensure_per_mutation_table db table =
  let* created =
    exec_steps db
      [
        Printf.sprintf
          "CREATE TABLE IF NOT EXISTS %s (action_id text PRIMARY KEY)"
          table;
      ]
  in
  match created with
  | Error err -> Lwt.return (Error err)
  | Ok () -> ensure_status_columns db table

let action_id_expr (module Db : Caqti_lwt.CONNECTION) table =
  let query =
    Caqti_request.Infix.(
      T.string ->! T.string
    ) "SELECT COALESCE((SELECT udt_name FROM information_schema.columns WHERE table_schema = current_schema() AND table_name = $1 AND column_name = 'action_id'), 'text')"
  in
  let* result = Db.find query table in
  match result with
  | Ok "uuid" -> Lwt.return (Ok "$1::uuid")
  | Ok _ -> Lwt.return (Ok "$1")
  | Error err -> Lwt.return (Error err)

let resolve_storage db ~mutation_name =
  let per_mutation_table =
    if is_safe_identifier_suffix mutation_name then Some (table_name mutation_name) else None
  in
  let* generic_exists = table_exists db "_resync_actions" in
  match generic_exists with
  | Ok true ->
    let* ensured = ensure_generic_table db in
    (match ensured with
     | Error err -> Lwt.return (Error (`Caqti err))
     | Ok () ->
       let* action_id_expr = action_id_expr db "_resync_actions" in
       (match action_id_expr with
        | Error err -> Lwt.return (Error (`Caqti err))
        | Ok action_id_expr -> Lwt.return (Ok (Generic_table { action_id_expr }))))
  | Error err -> Lwt.return (Error (`Caqti err))
  | Ok false ->
    match per_mutation_table with
    | Some table ->
      let* ensured = ensure_per_mutation_table db table in
      (match ensured with
       | Error err -> Lwt.return (Error (`Caqti err))
       | Ok () ->
         let* action_id_expr = action_id_expr db table in
         (match action_id_expr with
          | Error err -> Lwt.return (Error (`Caqti err))
          | Ok action_id_expr -> Lwt.return (Ok (Per_mutation_table { table; action_id_expr }))))
    | None ->
      Lwt.return
        (Error
           (`Msg
              (Printf.sprintf
                 "No action ledger table found for mutation %S. Expected _resync_actions or %s."
                 mutation_name
                 (match per_mutation_table with Some table -> table | None -> "<invalid mutation table name>"))))

let check (module Db : Caqti_lwt.CONNECTION) storage ~mutation_name ~action_id =
  let* result =
    match storage with
    | Per_mutation_table { table; _ } ->
      let query =
        Caqti_request.Infix.(
          T.string ->? T.(t2 string (option string))
        ) (Printf.sprintf "SELECT status, error_message FROM %s WHERE action_id::text = $1" table)
      in
      Db.find_opt query action_id
    | Generic_table _ ->
      let query =
        Caqti_request.Infix.(
          T.(t2 string string) ->? T.(t2 string (option string))
        ) "SELECT status, error_message FROM _resync_actions WHERE action_id::text = $1 AND mutation_name = $2"
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

let claim_action (module Db : Caqti_lwt.CONNECTION) storage ~mutation_name ~action_id =
  match storage with
  | Per_mutation_table { table; action_id_expr } ->
      Caqti_request.Infix.(
        T.string ->? T.(t2 string (option string))
      )
        (Printf.sprintf
           "INSERT INTO %s (action_id, status, error_message) VALUES (%s, 'ok', NULL) ON CONFLICT (action_id) DO NOTHING RETURNING status, error_message"
           table action_id_expr)
      |> fun query ->
      let* result = Db.find_opt query action_id in
      Lwt.return
        (match result with
         | Ok (Some _) -> Ok `Claimed
         | Ok None -> Ok `Duplicate
         | Error err -> Error err)
  | Generic_table { action_id_expr } ->
      Caqti_request.Infix.(
        T.(t2 string string) ->? T.(t2 string (option string))
      )
        (Printf.sprintf
           "INSERT INTO _resync_actions (action_id, mutation_name, status, error_message) VALUES (%s, $2, 'ok', NULL) ON CONFLICT (action_id) DO NOTHING RETURNING status, error_message"
           action_id_expr)
      |> fun query ->
      let* result = Db.find_opt query (action_id, mutation_name) in
      Lwt.return
        (match result with
         | Ok (Some _) -> Ok `Claimed
         | Ok None -> Ok `Duplicate
         | Error err -> Error err)

let update_status (module Db : Caqti_lwt.CONNECTION) storage ~mutation_name ~action_id ~status ~error_message =
  match storage with
  | Per_mutation_table { table; _ } ->
      Caqti_request.Infix.(
        T.(t3 string string (option string)) ->. T.unit
      )
        (Printf.sprintf
           "UPDATE %s SET status = $2, error_message = $3, processed_at = NOW() WHERE action_id::text = $1"
           table)
      |> fun query -> Db.exec query (action_id, status, error_message)
  | Generic_table _ ->
      Caqti_request.Infix.(
        T.(t4 string string string (option string)) ->. T.unit
      )
        "UPDATE _resync_actions SET status = $3, error_message = $4, processed_at = NOW() WHERE action_id::text = $1 AND mutation_name = $2"
      |> fun query -> Db.exec query (action_id, mutation_name, status, error_message)

let record_failure_status (module Db : Caqti_lwt.CONNECTION) storage ~mutation_name ~action_id ~error_message =
  match storage with
  | Per_mutation_table { table; action_id_expr } ->
      Caqti_request.Infix.(
        T.(t2 string (option string)) ->. T.unit
      )
        (Printf.sprintf
           "INSERT INTO %s (action_id, status, error_message) VALUES (%s, 'failed', $2) ON CONFLICT (action_id) DO UPDATE SET status = CASE WHEN %s.status = 'ok' THEN %s.status ELSE EXCLUDED.status END, error_message = CASE WHEN %s.status = 'ok' THEN %s.error_message ELSE EXCLUDED.error_message END, processed_at = CASE WHEN %s.status = 'ok' THEN %s.processed_at ELSE NOW() END"
           table action_id_expr table table table table table table)
      |> fun query -> Db.exec query (action_id, error_message)
  | Generic_table { action_id_expr } ->
      Caqti_request.Infix.(
        T.(t3 string string (option string)) ->. T.unit
      )
        (Printf.sprintf
           "INSERT INTO _resync_actions (action_id, mutation_name, status, error_message) VALUES (%s, $2, 'failed', $3) ON CONFLICT (action_id) DO UPDATE SET status = CASE WHEN _resync_actions.status = 'ok' THEN _resync_actions.status ELSE EXCLUDED.status END, error_message = CASE WHEN _resync_actions.status = 'ok' THEN _resync_actions.error_message ELSE EXCLUDED.error_message END, processed_at = CASE WHEN _resync_actions.status = 'ok' THEN _resync_actions.processed_at ELSE NOW() END"
           action_id_expr)
      |> fun query -> Db.exec query (action_id, mutation_name, error_message)

let record_failed (module Db : Caqti_lwt.CONNECTION) ~mutation_name ~action_id ~msg =
  let truncated = truncate_msg msg in
  Printf.eprintf "[sql_action_store] record_failed for %s.%s: %s\n%!" mutation_name action_id msg;
  let db_module = (module Db : Caqti_lwt.CONNECTION) in
  (* Clear any leaked aborted transaction before touching the action ledger. *)
  let* () = best_effort_rollback db_module in
  let* storage = resolve_storage db_module ~mutation_name in
  match storage with
  | Ok storage ->
    let* result =
      record_failure_status db_module storage ~mutation_name ~action_id ~error_message:(Some truncated)
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
  (* Clear any leaked aborted transaction before touching the action ledger. *)
  let* () = best_effort_rollback db_module in
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
        (*
          The action row is inserted before the callback so concurrent replays
          of the same action id block on the primary key. The savepoint keeps
          that claim intact even when user SQL aborts the mutation work.
        *)
        let* claim_result = claim_action db_module storage ~mutation_name ~action_id in
        (match claim_result with
         | Error err -> ack_caqti_error err
         | Ok `Duplicate ->
           let* replay_result = check db_module storage ~mutation_name ~action_id in
           let* () = best_effort_rollback db_module in
           (match replay_result with
            | Error err -> ack_error (Caqti_error.show err)
            | Ok `Already_ok -> Lwt.return (Ack (Ok ()))
            | Ok (`Already_failed msg) -> Lwt.return (Ack (Error msg))
            | Ok `New ->
              ack_error
                (Printf.sprintf
                   "Action %s.%s was claimed by another transaction but no committed result was found"
                   mutation_name action_id))
         | Ok `Claimed ->
           let finish_success result =
             let* release_result = exec_unit db_module "RELEASE SAVEPOINT resync_action_handler" in
             match release_result with
             | Error err -> ack_caqti_error err
             | Ok () ->
               let* commit_result = Db.commit () in
               (match commit_result with
                | Error err -> ack_caqti_error err
                | Ok () -> Lwt.return result)
           in
           let finish_failure msg =
             let truncated = truncate_msg msg in
             let* rollback_result = exec_unit db_module "ROLLBACK TO SAVEPOINT resync_action_handler" in
             match rollback_result with
             | Error err -> ack_caqti_error err
             | Ok () ->
               let* record_result =
                 update_status db_module storage ~mutation_name ~action_id
                   ~status:"failed" ~error_message:(Some truncated)
               in
               (match record_result with
                | Error err -> ack_caqti_error err
                | Ok () ->
                  let* commit_result = Db.commit () in
                  (match commit_result with
                   | Error err -> ack_caqti_error err
                   | Ok () -> Lwt.return (Ack (Error msg))))
           in
           let finish_noack () =
             let* rollback_result = Db.rollback () in
             match rollback_result with
             | Error err -> Lwt.return (Ack (Error (Caqti_error.show err)))
             | Ok () -> Lwt.return NoAck
           in
           let* savepoint_result = exec_unit db_module "SAVEPOINT resync_action_handler" in
           (match savepoint_result with
            | Error err -> ack_caqti_error err
            | Ok () ->
              Lwt.catch
                (fun () ->
                  let* result = callback () in
                  match result with
                  | Ack (Ok ()) -> finish_success (Ack (Ok ()))
                  | Ack_after_commit _ as result -> finish_success result
                  | Ack (Error msg) -> finish_failure msg
                  | NoAck -> finish_noack ())
                (fun exn -> finish_failure (Printexc.to_string exn))))

include (struct
  let with_guard = with_guard
  let record_failed = record_failed
end : Action_store.S)
