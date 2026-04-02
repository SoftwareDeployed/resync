let usage () =
  Printf.eprintf
    "Usage: realtime-schema-codegen --sql-dir DIR --out-triggers FILE --out-snapshot FILE --migrations-dir DIR\n";
  exit 1

let () =
  let sql_dir = ref None in
  let out_triggers = ref None in
  let out_snapshot = ref None in
  let migrations_dir = ref None in
  let set target value = target := Some value in
  let spec =
    [ ("--sql-dir", Arg.String (set sql_dir), "Directory containing annotated SQL files");
      ("--out-triggers", Arg.String (set out_triggers), "Output path for generated realtime SQL");
      ("--out-snapshot", Arg.String (set out_snapshot), "Output path for schema snapshot JSON");
      ("--migrations-dir", Arg.String (set migrations_dir), "Directory for timestamped migration files") ]
  in
  Arg.parse spec (fun _ -> usage ()) "realtime-schema-codegen";
  match (!sql_dir, !out_triggers, !out_snapshot, !migrations_dir) with
  | Some sql_dir, Some out_triggers, Some out_snapshot, Some migrations_dir ->
      let _ =
        Realtime_schema_codegen.write_generated_artifacts ~sql_dir ~triggers_path:out_triggers
          ~snapshot_path:out_snapshot ~migrations_dir
      in
      ()
  | _ -> usage ()
