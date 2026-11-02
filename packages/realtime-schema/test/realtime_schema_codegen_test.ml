let rec find_workspace_root directory =
  let marker = Filename.concat directory "dune-project" in
  if Sys.file_exists marker then
    directory
  else
    let parent = Filename.dirname directory in
    if parent = directory then directory else find_workspace_root parent

let read_file path =
  let channel = open_in_bin path in
  Fun.protect
    ~finally:(fun () -> close_in channel)
    (fun () -> really_input_string channel (in_channel_length channel))

(* The checked-in generated/realtime.sql files are the golden files: if the
   generator's output changes, this test fails until the artifacts are
   regenerated with `pnpm run <demo>:schema:generate`. *)
let golden_trigger_demos =
  List.map
    (fun demo ->
      (demo, Filename.concat (Filename.concat (Filename.concat "demos" demo) "server") "sql"))
    [ "ecommerce"; "llm-chat"; "todo-multiplayer" ]

let golden_triggers_test () =
  let root = find_workspace_root (Sys.getcwd ()) in
  List.iter
    (fun (demo, sql_dir) ->
      let schema = Realtime_schema_parser.parse_directory (Filename.concat root sql_dir) in
      let generated = Realtime_schema_codegen.generated_triggers_sql schema ^ "\n" in
      let golden_path =
        Filename.concat root (Filename.concat sql_dir (Filename.concat "generated" "realtime.sql"))
      in
      let golden = read_file golden_path in
      if generated <> golden then
        let path =
          Filename.concat (Filename.get_temp_dir_name ()) (demo ^ "-realtime-mismatch.sql")
        in
        let channel = open_out_bin path in
        output_string channel generated;
        close_out channel;
        let script =
          match demo with
          | "ecommerce" -> "schema:generate"
          | other -> other ^ ":schema:generate"
        in
        Alcotest.failf
          "generated triggers for %s drifted from %s (new output written to %s; regenerate with `pnpm run %s`)"
          demo golden_path path script
      else ())
    golden_trigger_demos

let write_temp_sql name contents =
  let dir =
    Filename.concat (Filename.get_temp_dir_name ()) ("realtime_schema_test_" ^ string_of_int (Unix.getpid ()))
  in
  if not (Sys.file_exists dir) then Unix.mkdir dir 0o755;
  let path = Filename.concat dir name in
  let channel = open_out_bin path in
  output_string channel contents;
  close_out channel;
  dir

let parse_string name contents =
  let path = write_temp_sql name contents in
  Realtime_schema_parser.parse_directory (Filename.dirname path)

let primary_key_implies_not_null_test () =
  let schema =
    parse_string "realtime_schema_pk_test.sql"
      {sql|-- @table
-- @id_column id
CREATE TABLE sample (
  id uuid PRIMARY KEY,
  name varchar NOT NULL,
  notes text
);|sql}
  in
  (match schema.Realtime_schema_types.tables with
  | table :: _ ->
    let by_name column_name (column : Realtime_schema_types.column) = column.name = column_name in
    (match List.find_opt (by_name "id") table.Realtime_schema_types.columns with
    | Some id_column ->
      Alcotest.(check bool) "PRIMARY KEY column is not nullable" false id_column.nullable;
      Alcotest.(check bool) "PRIMARY KEY detected" true id_column.primary_key
    | None -> Alcotest.fail "id column not parsed");
    (match List.find_opt (by_name "name") table.Realtime_schema_types.columns with
    | Some name_column -> Alcotest.(check bool) "NOT NULL column is not nullable" false name_column.nullable
    | None -> Alcotest.fail "name column not parsed");
    (match List.find_opt (by_name "notes") table.Realtime_schema_types.columns with
    | Some notes_column -> Alcotest.(check bool) "plain column is nullable" true notes_column.nullable
    | None -> Alcotest.fail "notes column not parsed")
  | [] -> Alcotest.fail "table not parsed")

let triggers_exclude_touch_blocks_test () =
  (* The generator must not emit `UPDATE premise` blocks: that was ecommerce
     specific and ran (and swallowed) an exception on every write in demos
     without a premise table. *)
  let root = find_workspace_root (Sys.getcwd ()) in
  List.iter
    (fun (_, sql_dir) ->
      let schema = Realtime_schema_parser.parse_directory (Filename.concat root sql_dir) in
      let generated = Realtime_schema_codegen.generated_triggers_sql schema in
      let contains_touch =
        try
          ignore (Str.search_forward (Str.regexp_string "UPDATE premise") generated 0);
          true
        with Not_found -> false
      in
      if contains_touch then
        Alcotest.fail ("generated triggers contain dead `UPDATE premise` touch block: " ^ sql_dir))
    golden_trigger_demos

let () =
  let open Alcotest in
  run "realtime_schema_codegen"
    [ ( "generated_triggers",
        [ test_case "matches checked-in golden files" `Quick golden_triggers_test
        ; test_case "contains no premise touch blocks" `Quick triggers_exclude_touch_blocks_test
        ] )
    ; ( "parser",
        [ test_case "PRIMARY KEY implies NOT NULL" `Quick primary_key_implies_not_null_test ] )
    ]
