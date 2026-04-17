open Ppxlib
open Ast_builder.Default

let rec find_workspace_root directory =
  let marker = Filename.concat directory "dune-project" in
  if Sys.file_exists marker then
    directory
  else
    let parent = Filename.dirname directory in
    if parent = directory then directory else find_workspace_root parent

let expand ~loc ~path:_ raw_path =
  let sql_dir =
    if Filename.is_relative raw_path then
      Filename.concat (find_workspace_root (Sys.getcwd ())) raw_path
    else
      raw_path
  in
  let schema = Realtime_schema_parser.parse_directory sql_dir in
  let structure = Realtime_schema_codegen.module_source schema ~loc in
  match structure with
  | [ item ] -> item
  | _ ->
    Location.raise_errorf ~loc
      "realtime_schema: expected generated code to produce exactly one \
       structure item, got %d"
      (List.length structure)

let ext =
  Extension.declare "realtime_schema" Extension.Context.structure_item
    Ast_pattern.(pstr (pstr_eval (estring __) nil ^:: nil))
    expand

let () = Driver.register_transformation ~extensions:[ ext ] "realtime_schema_ppx"
