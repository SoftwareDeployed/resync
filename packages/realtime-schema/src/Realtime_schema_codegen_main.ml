let usage () =
  Printf.eprintf
    "Usage: realtime-schema-codegen --sql-dir DIR --out-triggers FILE\n";
  exit 1

let () =
  let sql_dir = ref None in
  let out_triggers = ref None in
  let set target value = target := Some value in
  let spec =
    [ ("--sql-dir", Arg.String (set sql_dir), "Directory containing annotated SQL files");
      ("--out-triggers", Arg.String (set out_triggers), "Output path for generated realtime SQL") ]
  in
  Arg.parse spec (fun _ -> usage ()) "realtime-schema-codegen";
  match (!sql_dir, !out_triggers) with
  | Some sql_dir, Some out_triggers ->
      let _ = Realtime_schema_codegen.write_generated_artifacts ~sql_dir ~triggers_path:out_triggers in
      ()
  | _ -> usage ()
