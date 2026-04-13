open Realtime_schema_types
open Realtime_schema_utils

let annotation_value annotations key = List.assoc_opt key annotations

let annotation_values annotations key =
  annotations
  |> List.filter_map (fun (annotation_key, value) ->
         if annotation_key = key then Some value else None)

let strip_quotes value =
  let value = trim value in
  let length = String.length value in
  if length >= 2 then
    match (value.[0], value.[length - 1]) with
    | '"', '"' | '\'', '\'' -> String.sub value 1 (length - 2)
    | _ -> value
  else
    value

let normalize_whitespace value =
  let buffer = Buffer.create (String.length value) in
  let rec loop last_was_space index =
    if index >= String.length value then
      Buffer.contents buffer |> trim
    else
      let ch = value.[index] in
      let is_space =
        match ch with ' ' | '\n' | '\r' | '\t' -> true | _ -> false
      in
      if is_space then (
        if not last_was_space then Buffer.add_char buffer ' ';
        loop true (index + 1))
      else (
        Buffer.add_char buffer ch;
        loop false (index + 1))
  in
  loop true 0

let parse_annotation_line line =
  let trimmed = trim line in
  if starts_with ~prefix:"-- @" trimmed then
    let payload = String.sub trimmed 4 (String.length trimmed - 4) |> trim in
    match String.split_on_char ' ' payload with
    | [] -> None
    | key :: tail -> Some (lowercase key, String.concat " " tail |> trim)
  else
    None

let resolve_source_directory input_name =
  let marker = Filename.dir_sep ^ "_build" ^ Filename.dir_sep in
  try
    let marker_index = Str.search_forward (Str.regexp_string marker) input_name 0 in
    let root = String.sub input_name 0 marker_index in
    let suffix_start = marker_index + String.length marker in
    let remaining = String.sub input_name suffix_start (String.length input_name - suffix_start) in
    let next_sep = String.index remaining Filename.dir_sep.[0] in
    let source_relative =
      String.sub remaining (next_sep + 1) (String.length remaining - next_sep - 1)
    in
    Filename.dirname (Filename.concat root source_relative)
  with Not_found -> Filename.dirname input_name

let resolve_sql_dir ~input_name raw_path =
  if Filename.is_relative raw_path then
    Filename.concat (resolve_source_directory input_name) raw_path
  else
    raw_path

let parse_broadcast_channel value =
  let parse prefix constructor =
    if starts_with ~prefix value then
      let suffix =
        String.sub value (String.length prefix) (String.length value - String.length prefix)
        |> trim |> strip_quotes
      in
      Some (constructor suffix)
    else
      None
  in
  match parse "column=" (fun column -> Column column) with
  | Some _ as result -> result
  | None -> (
      match parse "computed=" (fun expr -> Computed expr) with
      | Some _ as result -> result
      | None -> (
          match parse "conditional=" (fun expr -> Conditional expr) with
          | Some _ as result -> result
          | None -> parse "subquery=" (fun expr -> Subquery expr)))

let parse_broadcast_parent value =
  let fields = String.split_on_char ' ' value |> List.filter (( <> ) "") in
  let lookup key =
    fields
    |> List.find_map (fun entry ->
           match String.split_on_char '=' entry with
           | [ entry_key; entry_value ] when lowercase entry_key = key ->
               Some (strip_quotes entry_value)
           | _ -> None)
  in
  match (lookup "table", lookup "query") with
  | Some parent_table, Some query_name -> Some { parent_table; query_name }
  | _ -> None

let parse_broadcast_to_views value =
  let fields = String.split_on_char ' ' value |> List.filter (( <> ) "") in
  let lookup key =
    fields
    |> List.find_map (fun entry ->
           match String.split_on_char '=' entry with
           | [ entry_key; entry_value ] when lowercase entry_key = key ->
               Some (strip_quotes entry_value)
           | _ -> None)
  in
  match (lookup "table", lookup "channel") with
  | Some view_table, Some channel_column -> Some { view_table; channel_column }
  | _ -> None

let find_matching_paren value open_index =
  let rec loop index depth in_single in_double =
    if index >= String.length value then
      raise Not_found
    else
      let ch = value.[index] in
      if ch = '\'' && not in_double then
        loop (index + 1) depth (not in_single) in_double
      else if ch = '"' && not in_single then
        loop (index + 1) depth in_single (not in_double)
      else if in_single || in_double then
        loop (index + 1) depth in_single in_double
      else if ch = '(' then
        loop (index + 1) (depth + 1) in_single in_double
      else if ch = ')' then
        if depth = 1 then index else loop (index + 1) (depth - 1) in_single in_double
      else
        loop (index + 1) depth in_single in_double
  in
  loop (open_index + 1) 1 false false

let parse_default fragment =
  let fragment = normalize_whitespace fragment in
  let keyword = "default " in
  match Str.search_forward (Str.regexp_case_fold keyword) fragment 0 with
  | exception Not_found -> None
  | index ->
      let start = index + String.length keyword in
      let suffix = String.sub fragment start (String.length fragment - start) in
      let stop_markers = [ " primary key"; " references"; " not null"; " unique"; " check"; " constraint" ] in
      let stop_index =
        stop_markers
        |> List.filter_map (fun marker ->
               try Some (Str.search_forward (Str.regexp_case_fold marker) suffix 0) with Not_found -> None)
        |> List.sort compare
        |> function head :: _ -> head | [] -> String.length suffix
      in
      Some (String.sub suffix 0 stop_index |> trim)

let parse_inline_foreign_key column_name fragment =
  let fragment = normalize_whitespace fragment in
  let regex =
    Str.regexp_case_fold {|references \([A-Za-z0-9_]+\) *(\([A-Za-z0-9_]+\))|}
  in
  try
    let _ = Str.search_forward regex fragment 0 in
    Some
      {
        column = column_name;
        referenced_table = Str.matched_group 1 fragment;
        referenced_column = Str.matched_group 2 fragment;
      }
  with Not_found -> None

let parse_column fragment =
  let normalized = normalize_whitespace fragment in
  let tokens = String.split_on_char ' ' normalized |> List.filter (( <> ) "") in
  match tokens with
  | [] -> None
  | name :: rest ->
      let stop_words =
        [ "not"; "null"; "default"; "primary"; "references"; "unique"; "check"; "constraint" ]
      in
      let rec take_type acc = function
        | token :: tail when not (List.mem (lowercase token) stop_words) -> take_type (token :: acc) tail
        | remaining -> List.rev acc, remaining
      in
      let type_tokens, _ = take_type [] rest in
      let sql_type_raw = String.concat " " type_tokens |> trim in
      if sql_type_raw = "" then
        None
      else
        Some
          {
            name;
            sql_type = sql_type_of_string sql_type_raw;
            sql_type_raw;
            nullable = not (Str.string_match (Str.regexp_case_fold {|.*not null.*|}) normalized 0);
            primary_key = Str.string_match (Str.regexp_case_fold {|.*primary key.*|}) normalized 0;
            default = parse_default fragment;
            foreign_key = parse_inline_foreign_key name fragment;
            definition_sql = trim fragment;
          }

let apply_table_constraints columns constraint_fragments =
  let fk_by_column = Hashtbl.create 8 in
  let composite_key = ref [] in
  List.iter
    (fun fragment ->
      let normalized = normalize_whitespace fragment in
      let pk_regex = Str.regexp_case_fold {|primary key *(\([^)]*\))|} in
      let fk_regex =
        Str.regexp_case_fold
          {|foreign key *(\([A-Za-z0-9_]+\)) references \([A-Za-z0-9_]+\) *(\([A-Za-z0-9_]+\))|}
      in
      if Str.string_match pk_regex normalized 0 then
        composite_key := split_top_level (Str.matched_group 1 normalized) |> List.map trim
      else if Str.string_match fk_regex normalized 0 then
        let column = Str.matched_group 1 normalized |> trim in
        let referenced_table = Str.matched_group 2 normalized |> trim in
        let referenced_column = Str.matched_group 3 normalized |> trim in
        Hashtbl.replace fk_by_column column { column; referenced_table; referenced_column })
    constraint_fragments;
  let columns =
    List.map
      (fun (column : column) ->
        match (column.foreign_key, Hashtbl.find_opt fk_by_column column.name) with
        | None, Some foreign_key -> { column with foreign_key = Some foreign_key }
        | _ -> column)
      columns
  in
  (columns, !composite_key)

let parse_table ~source_file ~annotations statement : table option =
  let header = normalize_whitespace statement in
  let header_regex = Str.regexp_case_fold {|create table\( if not exists\)? \([A-Za-z0-9_]+\)|} in
  if not (Str.string_match header_regex header 0) then
    None
  else
    let table_name = Str.matched_group 2 header |> trim in
    let open_index = String.index statement '(' in
    let close_index = find_matching_paren statement open_index in
    let body = String.sub statement (open_index + 1) (close_index - open_index - 1) in
    let fragments = split_top_level body in
    let column_fragments, constraint_fragments =
      List.partition
        (fun fragment ->
          let normalized = lowercase (normalize_whitespace fragment) in
          not
            (starts_with ~prefix:"primary key" normalized
            || starts_with ~prefix:"foreign key" normalized
            || starts_with ~prefix:"constraint" normalized))
        fragments
    in
    let columns = list_filter_map parse_column column_fragments in
    let columns, parsed_composite_key = apply_table_constraints columns constraint_fragments in
    let id_column =
      match annotation_value annotations "id_column" |> Option.map trim with
      | Some value -> Some value
      | None ->
          columns
          |> List.find_opt (fun (column : column) -> column.primary_key)
          |> Option.map (fun (column : column) -> column.name)
    in
    let composite_key =
      match annotation_value annotations "composite_key" with
      | Some value -> split_top_level value |> List.map trim
      | None -> parsed_composite_key
    in
    let broadcast_channel =
      match annotation_value annotations "broadcast_channel" with
      | Some value -> parse_broadcast_channel value
      | None -> None
    in
    let broadcast_parent =
      match annotation_value annotations "broadcast_parent" with
      | Some value -> parse_broadcast_parent value
      | None -> None
    in
    let broadcast_to_views =
      match annotation_value annotations "broadcast_to_views" with
      | Some value -> parse_broadcast_to_views value
      | None -> None
    in
    Some
      {
        name = table_name;
        columns;
        id_column;
        composite_key;
        broadcast_channel;
        broadcast_parent;
        broadcast_to_views;
        create_sql = strip_trailing_semicolons statement ^ ";";
        source_file;
      }

let alias_map_of_query sql =
  let normalized = normalize_whitespace sql in
  let alias_map = Hashtbl.create 8 in
  let regex =
    Str.regexp_case_fold {|\(from\|join\) \([A-Za-z0-9_]+\)\( +as\)? +\([A-Za-z0-9_]+\)|}
  in
  let rec loop position =
    try
      let next = Str.search_forward regex normalized position in
      let table_name = Str.matched_group 2 normalized in
      let alias = Str.matched_group 4 normalized in
      Hashtbl.replace alias_map alias table_name;
      loop (next + 1)
    with Not_found -> alias_map
  in
  loop 0

let infer_query_params tables sql =
  let pg_params = Realtime_schema_pg_query.infer_params tables sql in
  match pg_params with
  | _ :: _ -> pg_params
  | [] ->
  let normalized = normalize_whitespace sql in
  let alias_map = alias_map_of_query normalized in
  let sql_without_casts = Str.global_replace (Str.regexp {|::[A-Za-z0-9_]+|}) "" normalized in
  let bare_from_table =
    let regex = Str.regexp_case_fold {|from \([A-Za-z0-9_]+\)|} in
    try
      let _ = Str.search_forward regex normalized 0 in
      Some (Str.matched_group 1 normalized)
    with Not_found -> None
  in
  let update_table =
    let regex = Str.regexp_case_fold {|update \([A-Za-z0-9_]+\)|} in
    try
      let _ = Str.search_forward regex normalized 0 in
      Some (Str.matched_group 1 normalized)
    with Not_found -> None
  in
  let delete_table =
    let regex = Str.regexp_case_fold {|delete from \([A-Za-z0-9_]+\)|} in
    try
      let _ = Str.search_forward regex normalized 0 in
      Some (Str.matched_group 1 normalized)
    with Not_found -> None
  in
  let insert_table_and_columns =
    let regex = Str.regexp_case_fold {|insert into \([A-Za-z0-9_]+\) *\(([^)]+)\)|} in
    try
      let _ = Str.search_forward regex normalized 0 in
      let table_name = Str.matched_group 1 normalized in
      let cols_str = Str.matched_group 2 normalized in
      let cols_str =
        let trimmed = trim cols_str in
        if starts_with ~prefix:"(" trimmed && ends_with ~suffix:")" trimmed then
          String.sub trimmed 1 (String.length trimmed - 2) |> trim
        else
          trimmed
      in
      let cols = split_top_level ~separator:',' cols_str |> List.map trim in
      Some (table_name, cols)
    with Not_found -> None
  in
  let target_table =
    match insert_table_and_columns with
    | Some (table_name, _) -> Some table_name
    | None -> (
        match update_table with
        | Some _ -> update_table
        | None -> (
            match delete_table with
            | Some _ -> delete_table
            | None -> bare_from_table))
  in
  let params = Hashtbl.create 4 in
  let record_param ?column_ref index table_name column_name =
    let column_ref = match column_ref with Some r -> r | None -> Some (table_name, column_name) in
    let ocaml_type, sql_type =
      match List.find_opt (fun (table : table) -> table.name = table_name) tables with
      | Some table -> (
          match column_by_name table column_name with
          | Some column -> (ocaml_type_of_sql_type column.sql_type, column.sql_type_raw)
          | None -> ("string", "text"))
      | None -> ("string", "text")
    in
    let payload_key = match column_ref with Some (_, col) -> Some col | None -> None in
    Hashtbl.replace params index { index; column_ref; payload_key; ocaml_type; sql_type }
  in
  (* Handle INSERT ... VALUES ($1, $2, ...) by positional mapping to column list *)
  (match insert_table_and_columns with
  | Some (table_name, columns) -> (
      let values_regex = Str.regexp_case_fold {|values *(\([^)]+\))|} in
      try
        let _ = Str.search_forward values_regex sql_without_casts 0 in
        let values_str = Str.matched_group 1 sql_without_casts in
        let values = split_top_level ~separator:',' values_str |> List.map trim in
        List.iteri
          (fun idx value ->
            let placeholder_regex = Str.regexp {|\$\([0-9]+\)|} in
            try
              let _ = Str.search_forward placeholder_regex value 0 in
              let index = Str.matched_group 1 value |> int_of_string in
              let column_name = List.nth columns idx in
              record_param index table_name column_name
            with Not_found | Failure _ -> ())
          values
      with Not_found -> ())
  | None -> ());
  (* Handle table.column = $N *)
  let regex =
    Str.regexp_case_fold {|\([A-Za-z0-9_]+\)\.\([A-Za-z0-9_]+\) *= *\$\([0-9]+\)|}
  in
  let rec loop position =
    try
      let next = Str.search_forward regex sql_without_casts position in
      let alias = Str.matched_group 1 sql_without_casts in
      let column_name = Str.matched_group 2 sql_without_casts in
      let index = Str.matched_group 3 sql_without_casts |> int_of_string in
      if not (Hashtbl.mem params index) then (
        let table_name =
          match Hashtbl.find_opt alias_map alias with
          | Some table_name -> table_name
          | None -> Option.value target_table ~default:alias
        in
        record_param index table_name column_name);
      loop (next + 1)
    with Not_found -> ()
  in
  loop 0;
  (* Handle SET column = $N, WHERE column = $N, AND column = $N *)
  let rec scan_eq regex position =
    try
      let next = Str.search_forward regex sql_without_casts position in
      let column_name = Str.matched_group 1 sql_without_casts in
      let index = Str.matched_group 2 sql_without_casts |> int_of_string in
      if not (Hashtbl.mem params index) then
        match target_table with
        | Some table_name -> record_param index table_name column_name
        | None ->
            Hashtbl.replace params index { index; column_ref = None; payload_key = None; ocaml_type = "string"; sql_type = "text" };
      scan_eq regex (next + 1)
    with Not_found -> ()
  in
  let set_regex = Str.regexp_case_fold {|set \([A-Za-z0-9_]+\) *= *\$\([0-9]+\)|} in
  scan_eq set_regex 0;
  let where_regex = Str.regexp_case_fold {|where \([A-Za-z0-9_]+\) *= *\$\([0-9]+\)|} in
  scan_eq where_regex 0;
  let join_regex = Str.regexp_case_fold {|and \([A-Za-z0-9_]+\) *= *\$\([0-9]+\)|} in
  scan_eq join_regex 0;
  (* Catch any remaining $N placeholders without context *)
  let placeholder_regex = Str.regexp {|\$\([0-9]+\)|} in
  let rec scan_placeholders position =
    try
      let next = Str.search_forward placeholder_regex sql_without_casts position in
      let index = Str.matched_group 1 sql_without_casts |> int_of_string in
      if not (Hashtbl.mem params index) then
        Hashtbl.replace params index { index; column_ref = None; payload_key = None; ocaml_type = "string"; sql_type = "text" };
      scan_placeholders (next + 1)
    with Not_found -> ()
  in
  scan_placeholders 0;
  Hashtbl.to_seq_values params |> List.of_seq |> List.sort (fun a b -> compare a.index b.index)

let parse_handler annotations =
  match annotation_value annotations "handler" with
  | Some "ocaml" -> Ocaml
  | _ -> Sql

let parse_query ~source_file ~annotations tables statement : query option =
  match annotation_value annotations "query" with
  | None -> None
  | Some name ->
      let return_table =
        match Realtime_schema_pg_query.infer_return_table statement with
        | Some t -> Some t
        | None ->
            let normalized = normalize_whitespace statement in
            let regex = Str.regexp_case_fold {|from \([A-Za-z0-9_]+\)|} in
            try
              let _ = Str.search_forward regex normalized 0 in
              Some (Str.matched_group 1 normalized)
            with Not_found -> None
      in
      let json_columns =
        let from_plural =
          annotation_values annotations "json_columns"
          |> List.concat_map (fun value -> split_top_level value |> List.map trim)
        in
        let from_singular = annotation_values annotations "json_column" |> List.map trim in
        List.sort_uniq String.compare (from_plural @ from_singular)
      in
      Some
        {
          name = trim name;
          sql = strip_trailing_semicolons statement ^ ";";
          source_file;
          cache_key = annotation_value annotations "cache_key" |> Option.map trim;
          return_table;
          json_columns;
          params = infer_query_params tables statement;
          handler = parse_handler annotations;
        }

let parse_mutation ~source_file ~annotations tables statement : mutation option =
  match annotation_value annotations "mutation" with
  | None -> None
  | Some name ->
      Some
        {
          name = trim name;
          sql = strip_trailing_semicolons statement ^ ";";
          source_file;
          params = infer_query_params tables statement;
          handler = parse_handler annotations;
        }

let extract_block_comments content =
  let rec loop index acc =
    if index >= String.length content then
      List.rev acc
    else
      match Str.search_forward (Str.regexp_string "/*") content index with
      | exception Not_found -> List.rev acc
      | start_index ->
          let end_index = Str.search_forward (Str.regexp_string "*/") content (start_index + 2) in
          let block = String.sub content (start_index + 2) (end_index - start_index - 2) in
          loop (end_index + 2) (block :: acc)
  in
  loop 0 []

let strip_block_comments content =
  let buffer = Buffer.create (String.length content) in
  let rec loop index =
    if index >= String.length content then
      Buffer.contents buffer
    else if index + 1 < String.length content && content.[index] = '/' && content.[index + 1] = '*' then
      let end_index = Str.search_forward (Str.regexp_string "*/") content (index + 2) in
      loop (end_index + 2)
    else (
      Buffer.add_char buffer content.[index];
      loop (index + 1))
  in
  loop 0

type block_result =
  | BlockQuery of query
  | BlockMutation of mutation

let parse_query_block ~source_file tables block =
  let lines = String.split_on_char '\n' block in
  let annotations, sql_lines =
    List.fold_left
      (fun (annotations, sql_lines) line ->
        let trimmed = trim line in
        if starts_with ~prefix:"@" trimmed then
          match parse_annotation_line ("-- " ^ trimmed) with
          | Some annotation -> (annotations @ [ annotation ], sql_lines)
          | None -> (annotations, sql_lines)
        else if trimmed = "" then
          (annotations, sql_lines)
        else
          (annotations, sql_lines @ [ line ]))
      ([], []) lines
  in
  let statement = String.concat "\n" sql_lines |> trim in
  if statement = "" then
    None
  else
    match parse_query ~source_file ~annotations tables statement with
    | Some query -> Some (BlockQuery query)
    | None ->
        match parse_mutation ~source_file ~annotations tables statement with
        | Some mutation -> Some (BlockMutation mutation)
        | None -> None

let parse_file tables_acc queries_acc mutations_acc file_path =
  let content = read_file file_path in
  let lines = content |> strip_block_comments |> String.split_on_char '\n' in
  let finalize_statement annotations buffer tables queries mutations =
    let statement = Buffer.contents buffer |> trim in
    Buffer.clear buffer;
    if statement = "" then
      (tables, queries, mutations)
    else
      let tables =
        match parse_table ~source_file:(Filename.basename file_path) ~annotations statement with
        | Some table -> tables @ [ table ]
        | None -> tables
      in
      let queries =
        match parse_query ~source_file:(Filename.basename file_path) ~annotations tables statement with
        | Some query -> queries @ [ query ]
        | None -> queries
      in
      let mutations =
        match parse_mutation ~source_file:(Filename.basename file_path) ~annotations tables statement with
        | Some mutation -> mutations @ [ mutation ]
        | None -> mutations
      in
      (tables, queries, mutations)
  in
  let buffer = Buffer.create (String.length content) in
  let rec loop annotations tables queries mutations = function
    | [] -> finalize_statement annotations buffer tables queries mutations
    | line :: rest ->
        let trimmed = trim line in
        if trimmed = "" && Buffer.length buffer = 0 then
          loop annotations tables queries mutations rest
        else if starts_with ~prefix:"--" trimmed && Buffer.length buffer = 0 then
          let annotations =
            match parse_annotation_line trimmed with
            | Some annotation -> annotations @ [ annotation ]
            | None -> annotations
          in
          loop annotations tables queries mutations rest
        else if starts_with ~prefix:"--" trimmed then
          loop annotations tables queries mutations rest
        else (
          if Buffer.length buffer > 0 then Buffer.add_char buffer '\n';
          Buffer.add_string buffer line;
          if String.contains line ';' then
            let tables, queries, mutations = finalize_statement annotations buffer tables queries mutations in
            loop [] tables queries mutations rest
          else
            loop annotations tables queries mutations rest)
  in
  let tables, queries, mutations = loop [] tables_acc queries_acc mutations_acc lines in
  let block_results =
    extract_block_comments content
    |> list_filter_map (parse_query_block ~source_file:(Filename.basename file_path) tables)
  in
  let block_queries = List.filter_map (function BlockQuery q -> Some q | BlockMutation _ -> None) block_results in
  let block_mutations = List.filter_map (function BlockMutation m -> Some m | BlockQuery _ -> None) block_results in
  (tables, queries @ block_queries, mutations @ block_mutations)

let topological_tables tables =
  let table_map = Hashtbl.create (List.length tables) in
  List.iter (fun (table : table) -> Hashtbl.replace table_map table.name table) tables;
  let visited = Hashtbl.create (List.length tables) in
  let order = ref [] in
  let rec visit table_name =
    if not (Hashtbl.mem visited table_name) then (
      Hashtbl.add visited table_name true;
      match Hashtbl.find_opt table_map table_name with
      | Some table ->
          List.iter
            (fun (column : column) ->
              match column.foreign_key with
              | Some foreign_key -> visit foreign_key.referenced_table
              | None -> ())
            table.columns;
          order := table :: !order
      | None -> ())
  in
  List.iter (fun (table : table) -> visit table.name) tables;
  List.rev !order

let parse_directory directory =
  let files = list_sql_files directory in
  let tables, queries, mutations =
    List.fold_left
      (fun (tables, queries, mutations) file_path ->
         parse_file tables queries mutations file_path)
      ([], [], []) files
  in
  let tables = topological_tables tables in
  let hash_input =
    String.concat "\n--FILE--\n"
      (List.map (fun file_path -> file_path ^ "\n" ^ read_file file_path) files)
  in
  { tables; queries; mutations; source_files = List.map Filename.basename files; schema_hash = digest_string hash_input }
