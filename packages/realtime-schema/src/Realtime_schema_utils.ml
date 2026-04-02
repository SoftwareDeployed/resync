open Realtime_schema_types

let trim = String.trim

let lowercase = String.lowercase_ascii

let starts_with ~prefix value =
  let prefix_length = String.length prefix in
  String.length value >= prefix_length
  && String.sub value 0 prefix_length = prefix

let ends_with ~suffix value =
  let suffix_length = String.length suffix in
  let value_length = String.length value in
  value_length >= suffix_length
  && String.sub value (value_length - suffix_length) suffix_length = suffix

let strip_suffix ~suffix value =
  if ends_with ~suffix value then
    String.sub value 0 (String.length value - String.length suffix)
  else
    value

let rec strip_trailing_semicolons value =
  let trimmed = trim value in
  if ends_with ~suffix:";" trimmed then
    strip_trailing_semicolons (strip_suffix ~suffix:";" trimmed)
  else
    trimmed

let split_top_level ?(separator = ',') value =
  let rec loop index depth in_single in_double current acc =
    if index = String.length value then
      let final = Buffer.contents current |> trim in
      List.rev (if final = "" then acc else final :: acc)
    else
      let ch = value.[index] in
      let next_depth, next_single, next_double, should_split =
        if ch = '\'' && not in_double then
          depth, not in_single, in_double, false
        else if ch = '"' && not in_single then
          depth, in_single, not in_double, false
        else if in_single || in_double then
          depth, in_single, in_double, false
        else if ch = '(' then
          depth + 1, in_single, in_double, false
        else if ch = ')' then
          max 0 (depth - 1), in_single, in_double, false
        else if ch = separator && depth = 0 then
          depth, in_single, in_double, true
        else
          depth, in_single, in_double, false
      in
      if should_split then
        let piece = Buffer.contents current |> trim in
        Buffer.clear current;
        loop (index + 1) next_depth next_single next_double current
          (if piece = "" then acc else piece :: acc)
      else (
        Buffer.add_char current ch;
        loop (index + 1) next_depth next_single next_double current acc)
  in
  loop 0 0 false false (Buffer.create (String.length value)) []

let sanitize_identifier value =
  let buffer = Buffer.create (String.length value) in
  String.iter
    (fun ch ->
      match ch with
      | 'a' .. 'z' | 'A' .. 'Z' | '0' .. '9' | '_' -> Buffer.add_char buffer ch
      | _ -> Buffer.add_char buffer '_')
    value;
  let result = Buffer.contents buffer in
  let needs_prefix = result <> "" && result.[0] >= '0' && result.[0] <= '9' in
  let normalized = if needs_prefix then "t_" ^ result else result in
  if normalized = "" then "unnamed" else normalized

let module_name_of_table table_name =
  table_name
  |> split_top_level ~separator:'_'
  |> List.map (fun part ->
         if part = "" then
           ""
         else
           String.capitalize_ascii (lowercase part))
  |> String.concat ""

let type_name_of_table table_name = sanitize_identifier (lowercase table_name)

let sql_type_of_string raw_type =
  match lowercase (trim raw_type) with
  | "uuid" -> Uuid
  | "varchar" | "character varying" -> Varchar
  | "text" -> Text
  | "int" | "integer" -> Int
  | "bigint" -> Bigint
  | "boolean" | "bool" -> Boolean
  | "timestamp" | "timestamp without time zone" -> Timestamp
  | "timestamptz" | "timestamp with time zone" -> Timestamptz
  | "json" -> Json
  | "jsonb" -> Jsonb
  | other -> Custom other

let ocaml_type_of_sql_type = function
  | Uuid | Varchar | Text | Custom _ -> "string"
  | Int -> "int"
  | Bigint -> "int64"
  | Boolean -> "bool"
  | Timestamp | Timestamptz -> "float"
  | Json | Jsonb -> "Yojson.Safe.t"

let sql_type_to_string = function
  | Uuid -> "uuid"
  | Varchar -> "varchar"
  | Text -> "text"
  | Int -> "int"
  | Bigint -> "bigint"
  | Boolean -> "boolean"
  | Timestamp -> "timestamp"
  | Timestamptz -> "timestamptz"
  | Json -> "json"
  | Jsonb -> "jsonb"
  | Custom value -> value

let digest_string value = Digest.string value |> Digest.to_hex

let read_file file_path =
  let channel = open_in_bin file_path in
  Fun.protect
    ~finally:(fun () -> close_in channel)
    (fun () -> really_input_string channel (in_channel_length channel))

let list_sql_files directory =
  Sys.readdir directory
  |> Array.to_list
  |> List.filter (fun name -> ends_with ~suffix:".sql" name)
  |> List.filter (fun name -> lowercase name <> "triggers.sql")
  |> List.sort String.compare
  |> List.map (Filename.concat directory)

let quote_ocaml_string value = String.escaped value

let quote_sql_literal value =
  let buffer = Buffer.create (String.length value + 2) in
  String.iter
    (fun ch ->
      if ch = '\'' then Buffer.add_string buffer "''" else Buffer.add_char buffer ch)
    value;
  Buffer.contents buffer

let rec list_filter_map fn = function
  | [] -> []
  | head :: tail -> (
      match fn head with
      | Some value -> value :: list_filter_map fn tail
      | None -> list_filter_map fn tail)

let option_map default fn = function Some value -> fn value | None -> default

let table_by_name (schema : schema) table_name =
  List.find_opt (fun (table : table) -> table.name = table_name) schema.tables

let column_by_name (table : table) column_name =
  List.find_opt (fun (column : column) -> column.name = column_name) table.columns

let string_of_broadcast_channel = function
  | Column value -> "column=" ^ value
  | Computed value -> "computed=" ^ value
  | Conditional value -> "conditional=" ^ value
  | Subquery value -> "subquery=" ^ value

let snapshot_of_schema (schema : schema) =
  let tables =
    List.map
      (fun (table : table) ->
        {
          table_name = table.name;
          columns = table.columns;
          id_column = table.id_column;
          composite_key = table.composite_key;
        })
      schema.tables
  in
  { schema_hash = schema.schema_hash; tables }
