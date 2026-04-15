open Realtime_schema_types
open Realtime_schema_utils

let indent block =
  block
  |> String.split_on_char '\n'
  |> List.map (fun line -> if trim line = "" then "" else "  " ^ line)
  |> String.concat "\n"

let string_list_literal values =
  values
  |> List.map (fun value -> Printf.sprintf "%S" value)
  |> String.concat "; "
  |> Printf.sprintf "[%s]"

let sql_string_literal value = Printf.sprintf "'%s'" (quote_sql_literal value)

let rec ocaml_of_sql_type = function
  | Uuid -> "Uuid"
  | Varchar -> "Varchar"
  | Text -> "Text"
  | Int -> "Int"
  | Bigint -> "Bigint"
  | Boolean -> "Boolean"
  | Timestamp -> "Timestamp"
  | Timestamptz -> "Timestamptz"
  | Json -> "Json"
  | Jsonb -> "Jsonb"
  | Custom value -> Printf.sprintf "Custom %S" value

let ocaml_of_foreign_key = function
  | None -> "None"
  | Some foreign_key ->
      Printf.sprintf
        "Some { column = %S; referenced_table = %S; referenced_column = %S }"
        foreign_key.column foreign_key.referenced_table foreign_key.referenced_column

let ocaml_of_broadcast_channel = function
  | None -> "None"
  | Some (Column value) -> Printf.sprintf "Some (Column %S)" value
  | Some (Computed value) -> Printf.sprintf "Some (Computed %S)" value
  | Some (Conditional value) -> Printf.sprintf "Some (Conditional %S)" value
  | Some (Subquery value) -> Printf.sprintf "Some (Subquery %S)" value

let ocaml_of_broadcast_parent = function
  | None -> "None"
  | Some parent ->
      Printf.sprintf
        "Some { parent_table = %S; query_name = %S }"
        parent.parent_table parent.query_name

let ocaml_of_broadcast_to_views = function
  | None -> "None"
  | Some config ->
      Printf.sprintf
        "Some { view_table = %S; channel_column = %S }"
        config.view_table config.channel_column

let column_record_literal (column : column) =
  Printf.sprintf
    "{ name = %S; sql_type = %s; sql_type_raw = %S; nullable = %b; primary_key = %b; default = %s; foreign_key = %s; definition_sql = %S }"
    column.name (ocaml_of_sql_type column.sql_type) column.sql_type_raw column.nullable
    column.primary_key
    (match column.default with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    (ocaml_of_foreign_key column.foreign_key)
    column.definition_sql

let table_record_literal (table : table) =
  let columns_literal =
    table.columns |> List.map column_record_literal |> String.concat "; " |> Printf.sprintf "[%s]"
  in
  Printf.sprintf
    "{ name = %S; columns = %s; id_column = %s; composite_key = %s; broadcast_channel = %s; broadcast_parent = %s; broadcast_to_views = %s; create_sql = %S; source_file = %S }"
    table.name columns_literal
    (match table.id_column with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    (string_list_literal table.composite_key)
    (ocaml_of_broadcast_channel table.broadcast_channel)
    (ocaml_of_broadcast_parent table.broadcast_parent)
    (ocaml_of_broadcast_to_views table.broadcast_to_views)
    table.create_sql table.source_file

let string_of_handler = function Sql -> "Sql" | Ocaml -> "Ocaml"

let caqti_type_of_ocaml_type = function
  | "string" -> "Caqti_type.string"
  | "int" -> "Caqti_type.int"
  | "int64" -> "Caqti_type.int64"
  | "bool" -> "Caqti_type.bool"
  | "float" -> "Caqti_type.float"
  | _ -> "Caqti_type.string"

let caqti_type_constructor_of_sql_type = function
  | Uuid | Varchar | Text | Json | Jsonb | Custom _ -> "Caqti_type.string"
  | Int -> "Caqti_type.int"
  | Bigint -> "Caqti_type.int64"
  | Boolean -> "Caqti_type.bool"
  | Timestamp | Timestamptz -> "Caqti_type.float"

let mutation_param_type_expr params =
  let type_of_param p = caqti_type_of_ocaml_type p.ocaml_type in
  match params with
  | [] -> "Caqti_type.unit"
  | [ p ] -> type_of_param p
  | [ p1; p2 ] -> Printf.sprintf "Caqti_type.t2 %s %s" (type_of_param p1) (type_of_param p2)
  | [ p1; p2; p3 ] -> Printf.sprintf "Caqti_type.t3 %s %s %s" (type_of_param p1) (type_of_param p2) (type_of_param p3)
  | [ p1; p2; p3; p4 ] ->
      Printf.sprintf "Caqti_type.t4 %s %s %s %s" (type_of_param p1) (type_of_param p2) (type_of_param p3) (type_of_param p4)
  | [ p1; p2; p3; p4; p5 ] ->
      Printf.sprintf "Caqti_type.t5 %s %s %s %s %s"
        (type_of_param p1) (type_of_param p2) (type_of_param p3) (type_of_param p4) (type_of_param p5)
  | _ -> "Caqti_type.unit"

let params_literal params =
  params
  |> List.map (fun param ->
         Printf.sprintf
           "{ index = %d; column_ref = %s; payload_key = %s; ocaml_type = %S; sql_type = %S }"
           param.index
           (match param.column_ref with
           | None -> "None"
           | Some (table_name, column_name) ->
               Printf.sprintf "Some (%S, %S)" table_name column_name)
           (match param.payload_key with None -> "None" | Some key -> Printf.sprintf "Some %S" key)
           param.ocaml_type param.sql_type)
  |> String.concat "; "
  |> Printf.sprintf "[%s]"

let query_record_literal (query : query) =
  let json_columns_literal = string_list_literal query.json_columns in
  Printf.sprintf
    "{ name = %S; sql = %S; source_file = %S; cache_key = %s; return_table = %s; json_columns = %s; params_metadata = %s; handler = %s }"
    query.name query.sql query.source_file
    (match query.cache_key with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    (match query.return_table with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    json_columns_literal (params_literal query.params) (string_of_handler query.handler)

let mutation_record_literal (mutation : mutation) =
  Printf.sprintf
    "{ name = %S; sql = %S; source_file = %S; params_metadata = %s; handler = %s }"
    mutation.name mutation.sql mutation.source_file
    (params_literal mutation.params) (string_of_handler mutation.handler)

let ocaml_type_string_of_sql_type sql_type =
  match ocaml_type_of_sql_type sql_type with
  | "string" -> "string"
  | "int" -> "int"
  | "int64" -> "int64"
  | "bool" -> "bool"
  | "float" -> "float"
  | "Yojson.Safe.t" -> "Yojson.Safe.t"
  | other -> other

let column_type_annotation (column : column) =
  ocaml_type_string_of_sql_type column.sql_type

let record_type_declaration (table : table) =
  let fields =
    table.columns
    |> List.map (fun (column : column) -> Printf.sprintf "  %s : %s;" (sanitize_identifier column.name) (column_type_annotation column))
    |> String.concat "\n"
  in
  Printf.sprintf "type %s = {\n%s\n}" (type_name_of_table table.name) fields

let caqti_type_for_columns ~type_name (columns : (string * sql_type) list) =
  match columns with
  | [] -> "let caqti_type = Caqti_type.unit"
  | _ ->
      let field_names = List.map (fun (name, _) -> sanitize_identifier name) columns in
      let constructor_args = String.concat " " field_names in
      let record_body =
        field_names
        |> List.map (fun name -> Printf.sprintf "%s = %s;" name name)
        |> String.concat " "
      in
      let proj_chain =
        columns
        |> List.map (fun (name, sql_type) ->
               Printf.sprintf "    @@ Caqti_type.proj %s (fun (r : %s) -> r.%s)"
                 (caqti_type_constructor_of_sql_type sql_type)
                 type_name
                 (sanitize_identifier name))
        |> String.concat "\n"
      in
      Printf.sprintf
        "let caqti_type =\n    Caqti_type.product(fun %s -> { %s })\n%s\n    @@ Caqti_type.proj_end [@@platform native]"
        constructor_args record_body proj_chain

let ocaml_type_to_json_fn ocaml_type =
  match ocaml_type with
  | "string" | "Uuid" | "Varchar" | "Text" -> "string_to_json"
  | "int" | "Int" | "Bigint" -> "int_to_json"
  | "bool" | "Boolean" -> "bool_to_json"
  | "float" | "Float" | "Timestamp" | "Timestamptz" -> "float_to_json"
  | _ -> "string_to_json"

let ocaml_type_of_json_fn ocaml_type =
  match ocaml_type with
  | "string" | "Uuid" | "Varchar" | "Text" -> "string_of_json"
  | "int" | "Int" | "Bigint" -> "int_of_json"
  | "bool" | "Boolean" -> "bool_of_json"
  | "float" | "Float" | "Timestamp" | "Timestamptz" -> "float_of_json"
  | _ -> "string_of_json"

let string_conversion_fn ocaml_type =
  match ocaml_type with
  | "string" | "Uuid" | "Varchar" | "Text" -> ""
  | "int" | "Int" | "Bigint" -> "string_of_int"
  | "bool" | "Boolean" -> "string_of_bool"
  | "float" | "Float" | "Timestamp" | "Timestamptz" -> "string_of_float"
  | _ -> ""

let params_type_declaration (params : query_param list) =
  match params with
  | [] -> "type params = unit"
  | _ ->
  let fields =
    params
    |> List.map (fun (param : query_param) ->
      let field_name =
        match param.payload_key with
        | Some key -> sanitize_identifier key
        | None -> Printf.sprintf "param_%d" param.index
      in
      Printf.sprintf " %s : %s;" field_name param.ocaml_type)
    |> String.concat "\n"
  in
  Printf.sprintf "type params = {\n%s\n}" fields

let encode_params_code (params : query_param list) =
  match params with
  | [] -> "let encodeParams () = Melange_json.Primitives.unit_to_json ()"
  | _ ->
  let dict_entries =
    params
    |> List.map (fun (param : query_param) ->
      let field_name =
        match param.payload_key with
        | Some key -> sanitize_identifier key
        | None -> Printf.sprintf "param_%d" param.index
      in
      let json_fn = ocaml_type_to_json_fn param.ocaml_type in
      Printf.sprintf "Js.Dict.set dict %S (Melange_json.Primitives.%s p.%s);"
        field_name json_fn field_name)
    |> String.concat "\n "
  in
    Printf.sprintf
    "let encodeParams (p: params) =\n let dict = Js.Dict.empty () in\n %s\n Melange_json.declassify (`Assoc (Array.to_list (Js.Dict.entries dict)))"
    dict_entries

let params_hash_code (params : query_param list) =
  match params with
  | [] -> "let paramsHash () = \"\""
  | [param] ->
  let field_name =
    match param.payload_key with
    | Some key -> sanitize_identifier key
    | None -> Printf.sprintf "param_%d" param.index
  in
  let string_conv = string_conversion_fn param.ocaml_type in
  if string_conv = "" then
    Printf.sprintf "let paramsHash (p: params) = p.%s" field_name
  else
    Printf.sprintf "let paramsHash (p: params) = %s(p.%s)" string_conv field_name
  | _ ->
  let hash_parts =
    params
    |> List.map (fun (param : query_param) ->
      let field_name =
        match param.payload_key with
        | Some key -> sanitize_identifier key
        | None -> Printf.sprintf "param_%d" param.index
      in
      let string_conv = string_conversion_fn param.ocaml_type in
      if string_conv = "" then
        Printf.sprintf "p.%s" field_name
      else
        Printf.sprintf "%s(p.%s)" string_conv field_name)
    |> String.concat " ^ \":\" ^ "
  in
  Printf.sprintf "let paramsHash (p: params) = %s" hash_parts

let caqti_param_expr (params : query_param list) =
  match params with
  | [] -> "()"
  | [ param ] ->
      let field_name =
        match param.payload_key with
        | Some key -> sanitize_identifier key
        | None -> Printf.sprintf "param_%d" param.index
      in
      Printf.sprintf "p.%s" field_name
  | params ->
      let fields =
        params
        |> List.map (fun (param : query_param) ->
               let field_name =
                 match param.payload_key with
                 | Some key -> sanitize_identifier key
                 | None -> Printf.sprintf "param_%d" param.index
               in
               Printf.sprintf "p.%s" field_name)
        |> String.concat ", "
      in
      Printf.sprintf "(%s)" fields

let channel_function_code tables (query : query) =
  match query.return_table with
  | None -> "let channel _ = \"\""
  | Some table_name -> (
    match List.find_opt (fun (t : table) -> t.name = table_name) tables with
    | None -> "let channel _ = \"\""
    | Some table -> (
      match table.broadcast_channel with
      | None -> "let channel _ = \"\""
      | Some (Column column_name) -> (
        match
          List.find_opt
            (fun (param : query_param) ->
              match param.column_ref with
              | Some (t_name, c_name) -> t_name = table_name && c_name = column_name
              | None -> false)
            query.params
        with
        | None -> "let channel _ = \"\""
        | Some param ->
          let field_name =
            match param.payload_key with
            | Some key -> sanitize_identifier key
            | None -> Printf.sprintf "param_%d" param.index
          in
          Printf.sprintf "let channel (p: params) = p.%s" field_name)
      | Some _ -> "let channel _ = \"\""))

let decode_row_code (columns : (string * sql_type) list) =
  match columns with
  | [] -> "let decodeRow _ = ()"
  | _ ->
      let field_decodes =
        columns
        |> List.map (fun (name, sql_type) ->
               let field_name = sanitize_identifier name in
               let ocaml_type = ocaml_type_string_of_sql_type sql_type in
               let json_fn = ocaml_type_of_json_fn ocaml_type in
               Printf.sprintf
" %s = StoreJson.requiredField ~json ~fieldName:%S ~decode:Melange_json.Primitives.%s;"
                 field_name name json_fn)
        |> String.concat "\n"
      in
      Printf.sprintf
        "let decodeRow json = {\n%s\n}"
        field_decodes

let row_to_json_code (columns : (string * sql_type) list) =
  match columns with
  | [] -> "let row_to_json _ = Melange_json.Primitives.unit_to_json ()"
  | _ ->
      let dict_entries =
        columns
        |> List.map (fun (name, sql_type) ->
               let field_name = sanitize_identifier name in
               let ocaml_type = ocaml_type_string_of_sql_type sql_type in
               let json_fn = ocaml_type_to_json_fn ocaml_type in
               Printf.sprintf
                 "Js.Dict.set dict %S (Melange_json.Primitives.%s row.%s);"
                 name json_fn field_name)
        |> String.concat "\n  "
      in
      Printf.sprintf
        "let row_to_json (row: row) =\n  let dict = Js.Dict.empty () in\n  %s\n  Melange_json.declassify (`Assoc (Array.to_list (Js.Dict.entries dict)))"
        dict_entries

let query_module_declaration tables (query : query) =
  let params_metadata_literal =
    query.params
    |> List.map (fun param -> Printf.sprintf "(%d, %S, %S)" param.index param.ocaml_type param.sql_type)
    |> String.concat "; "
  in
  let inferred_columns = Realtime_schema_pg_query.infer_select_columns tables query.return_table query.sql in
  let row_type_and_caqti =
    match inferred_columns with
    | Some columns when columns <> [] ->
        let fields =
          columns
          |> List.map (fun (name, sql_type) ->
                 Printf.sprintf "  %s : %s;" (sanitize_identifier name) (ocaml_type_string_of_sql_type sql_type))
          |> String.concat "\n"
        in
        (* row type is SHARED - no [@@platform native] *)
        let type_decl = Printf.sprintf "type row = {\n%s\n}" fields in
        let caqti_decl = caqti_type_for_columns ~type_name:"row" columns in
        Printf.sprintf "%s\n\n  %s [@@platform native]" type_decl caqti_decl
    | _ -> ""
  in
  let params_type_decl = params_type_declaration query.params in
  let channel_func = channel_function_code tables query in
  let params_hash_func = params_hash_code query.params in
  let encode_params_func = encode_params_code query.params in
  let decode_row_func =
    match inferred_columns with
    | Some columns when columns <> [] -> decode_row_code columns
    | _ -> "let decodeRow _ = ()"
  in
  let row_to_json_func =
    match inferred_columns with
    | Some columns when columns <> [] -> row_to_json_code columns
    | _ -> "let row_to_json _ = Melange_json.Primitives.unit_to_json ()"
  in
  let caqti_items =
 let param_type_expr = mutation_param_type_expr query.params in
 Printf.sprintf
 "let param_type = %s [@@platform native]\n\
  \ let request row_type = Caqti_request.Infix.(param_type ->* row_type)(sql) [@@platform native]\n\
  \ let find_request row_type = Caqti_request.Infix.(param_type ->? row_type)(sql) [@@platform native]\n\
  \ let collect (module Db : Caqti_lwt.CONNECTION) row_type param_values =\n\
  \ let open Lwt.Syntax in\n\
  \ let* result = Db.collect_list (request row_type) param_values in\n\
  \ Caqti_lwt.or_fail result [@@platform native]\n\
  \ let find_opt (module Db : Caqti_lwt.CONNECTION) row_type param_values =\n\
  \ let open Lwt.Syntax in\n\
  \ let* result = Db.find_opt (find_request row_type) param_values in\n\
  \ Caqti_lwt.or_fail result [@@platform native]"
 param_type_expr
  in
let caqti_args = caqti_param_expr query.params in
  let execute_func =
  Printf.sprintf
  "let execute (module Db : Caqti_lwt.CONNECTION) (p : params) =\n\
   \ let open Lwt.Syntax in\n\
   \ Lwt.catch\n\
   \ (fun () ->\n\
   \ let* rows = collect (module Db) caqti_type %s in\n\
   \ Lwt.return (Ok (Array.of_list rows)))\n\
   \ (function\n\
   \ | Caqti_error.Exn error -> Lwt.return (Error (Caqti_error.show error))\n\
   \ | exn -> Lwt.return (Error (Printexc.to_string exn)))\n\
   \ [@@platform native]"
  caqti_args
  in
  let new_items =
    Printf.sprintf "%s\n\n %s\n\n %s\n\n %s\n\n %s\n\n %s\n\n %s"
    params_type_decl channel_func params_hash_func encode_params_func decode_row_func row_to_json_func execute_func
  in
  Printf.sprintf
    "module %s = struct\n let name = %S\n let sql = %S\n let params_metadata = [%s]\n let return_table = %s\n let json_columns = %s\n let handler = %s\n\n %s\n\n %s\n\n %s\nend"
    (module_name_of_table query.name) query.name query.sql params_metadata_literal
  (match query.return_table with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
  (string_list_literal query.json_columns) (string_of_handler query.handler)
  row_type_and_caqti
  caqti_items
  new_items

let json_extractor_code ~key ~ocaml_type =
  let extractor =
    match ocaml_type with
    | "bool" ->
        "        | Some (`Bool b) -> Ok b\n\
        \        | Some (`String \"true\") -> Ok true\n\
        \        | Some (`String \"false\") -> Ok false\n\
        \        | _ -> Error msg"
    | "int" ->
        "        | Some (`Int i) -> Ok i\n\
        \        | Some (`Float f) -> Ok (int_of_float f)\n\
        \        | Some (`String s) -> (try Ok (int_of_string s) with Failure _ -> Error msg)\n\
        \        | _ -> Error msg"
    | "float" ->
        "        | Some (`Float f) -> Ok f\n\
        \        | Some (`Int i) -> Ok (float_of_int i)\n\
        \        | Some (`String s) -> (try Ok (float_of_string s) with Failure _ -> Error msg)\n\
        \        | _ -> Error msg"
    | "int64" ->
        "        | Some (`Int i) -> Ok (Int64.of_int i)\n\
        \        | Some (`String s) -> (try Ok (Int64.of_string s) with Failure _ -> Error msg)\n\
        \        | _ -> Error msg"
    | _ ->
        "        | Some (`String s) -> Ok s\n\
        \        | _ -> Error msg"
  in
  Printf.sprintf
    "let msg = \"Missing or invalid field: %s\" in\n\
    \  match payload with\n\
    \  | `Assoc fields -> (\n\
    \      match List.assoc_opt %S fields with\n\
    %s)\n\
    \  | _ -> Error msg"
    key key extractor

let dispatch_function_for_mutation (mutation : mutation) =
  match mutation.handler with
  | Ocaml -> ""
  | Sql ->
      let param_extractions =
        mutation.params
        |> List.map (fun param ->
               let key = Option.value param.payload_key ~default:(Printf.sprintf "param_%d" param.index) in
               let var_name = sanitize_identifier key in
               let extractor = json_extractor_code ~key ~ocaml_type:param.ocaml_type in
               Printf.sprintf "  let %s =\n%s\n  in" var_name (indent extractor))
        |> String.concat "\n"
      in
      let var_names =
        mutation.params
        |> List.map (fun param ->
               let key = Option.value param.payload_key ~default:(Printf.sprintf "param_%d" param.index) in
               sanitize_identifier key)
      in
      let tuple_expr =
        match var_names with
        | [] -> "()"
        | [ v ] -> v
        | vs -> Printf.sprintf "(%s)" (String.concat ", " vs)
      in
      let rec build_matches = function
        | [] ->
            Printf.sprintf
              "      Lwt.catch\n\
              \        (fun () ->\n\
              \           let open Lwt.Syntax in\n\
              \           let* () = exec (module Db) %s in\n\
              \           Lwt.return (Ok ()))\n\
              \        (function\n\
              \         | Caqti_error.Exn error -> Lwt.return (Error (Caqti_error.show error))\n\
              \         | exn -> Lwt.return (Error (Printexc.to_string exn)))"
              tuple_expr
        | v :: rest ->
            Printf.sprintf
              "  match %s with\n\
              \  | Error msg -> Lwt.return (Error msg)\n\
              \  | Ok %s ->\n%s"
              v v (indent (build_matches rest))
      in
      let body = build_matches var_names in
      let payload_binding =
        if mutation.params = [] then ""
        else
          "  let payload =\n\
          \    match action with\n\
          \    | `Assoc fields -> (\n\
          \        match List.assoc_opt \"payload\" fields with\n\
          \        | Some p -> p\n\
          \        | None -> `Assoc [])\n\
          \    | _ -> `Assoc []\n\
          \  in\n"
      in
      Printf.sprintf
        "let dispatch (module Db : Caqti_lwt.CONNECTION) action =\n\
        %s\
        %s\n\
        %s\n\
        [@@platform native]"
        payload_binding param_extractions body

let mutation_module_declaration (mutation : mutation) =
  let params_metadata_literal =
    mutation.params
    |> List.map (fun param -> Printf.sprintf "(%d, %S, %S)" param.index param.ocaml_type param.sql_type)
    |> String.concat "; "
  in
 let caqti_items =
 let param_type_expr = mutation_param_type_expr mutation.params in
 Printf.sprintf
 "let param_type = %s [@@platform native]\n\
  \ let request = Caqti_request.Infix.(param_type ->. Caqti_type.unit)(sql) [@@platform native]\n\
  \ let exec (module Db : Caqti_lwt.CONNECTION) param_values =\n\
  \ let open Lwt.Syntax in\n\
  \ let* result = Db.exec request param_values in\n\
  \ Caqti_lwt.or_fail result [@@platform native]"
 param_type_expr
  in
  let dispatch_items = dispatch_function_for_mutation mutation in
  let items =
    if dispatch_items = "" then caqti_items
    else Printf.sprintf "%s\n\n  %s" caqti_items dispatch_items
  in
  let params_type_decl = params_type_declaration mutation.params in
  let encode_params_func = encode_params_code mutation.params in
  Printf.sprintf
    "module %s = struct\n let name = %S\n let sql = %S\n let params_metadata = [%s]\n let handler = %s\n\n %s\n\n %s\n\n %s\nend"
    (module_name_of_table mutation.name) mutation.name mutation.sql params_metadata_literal
    (string_of_handler mutation.handler)
    items
    params_type_decl
    encode_params_func

let table_id_expression row_alias (table : table) =
  match table.id_column with
  | Some id_column -> Printf.sprintf "to_jsonb(%s.%s)" row_alias id_column
  | None ->
      let pieces =
        table.composite_key
        |> List.map (fun column_name -> Printf.sprintf "%S, %s.%s" column_name row_alias column_name)
        |> String.concat ", "
      in
      if pieces = "" then
        "NULL"
      else
        Printf.sprintf "jsonb_build_object(%s)" pieces

let direct_channel_assignment (table : table) =
  match table.broadcast_channel with
  | Some (Column column_name) ->
      Some
        (Printf.sprintf
           "channel_name := CASE WHEN TG_OP = 'DELETE' THEN OLD.%s::text ELSE NEW.%s::text END;"
           column_name column_name)
  | Some (Computed expression) | Some (Conditional expression) ->
      Some
        (Printf.sprintf
           "SELECT (%s)::text INTO channel_name FROM (SELECT CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END) AS current_row;"
           expression)
  | Some (Subquery query_sql) ->
      Some
        (Printf.sprintf
           "SELECT generated_channel.channel_name::text INTO channel_name FROM (%s) AS generated_channel(channel_name) LIMIT 1;"
           query_sql)
  | None -> None

let maybe_touch_premise column_name_expr =
  Printf.sprintf
    "IF channel_name IS NOT NULL THEN\n    BEGIN\n      UPDATE premise SET updated_at = NOW() WHERE id = %s::uuid;\n    EXCEPTION WHEN undefined_table THEN\n      NULL;\n    END;\n  END IF;"
    column_name_expr

let current_id_expression (table : table) = table_id_expression "NEW" table

let deleted_id_expression (table : table) = table_id_expression "OLD" table

let list_hd_opt = function head :: _ -> Some head | [] -> None

let parent_id_column (table : table) =
  match table.id_column with
  | Some column_name -> Some column_name
  | None -> list_hd_opt table.composite_key

let delete_channel_lookup_sql (parent_table : table) =
  match (parent_table.broadcast_channel, parent_id_column parent_table) with
  | Some (Column column_name), Some id_column ->
      Printf.sprintf
        "SELECT %s::text INTO channel_name FROM %s WHERE %s = parent_row_id;"
        column_name parent_table.name id_column
  | _ -> "channel_name := NULL;"

let embedded_query_sql sql = strip_trailing_semicolons sql

let replace_first_param sql replacement =
  Str.global_replace (Str.regexp_string "$1") replacement sql

let query_data_assignment_sql record_name (query : query) =
  let base = "payload_data := to_jsonb(" ^ record_name ^ ");" in
  query.json_columns
  |> List.fold_left
       (fun statements column_name ->
         statements
         @ [ Printf.sprintf
               "payload_data := jsonb_set(payload_data, '{%s}', COALESCE((%s.%s)::jsonb, 'null'::jsonb), true);"
               column_name record_name column_name ])
       [ base ]
  |> String.concat "\n  "

let ready_query_for_table (schema : schema) table_name =
  schema.tables
  |> List.find_map (fun (child_table : table) ->
         match child_table.broadcast_parent with
         | Some parent when parent.parent_table = table_name ->
             List.find_opt (fun (query : query) -> query.name = parent.query_name) schema.queries
         | _ -> None)

let direct_trigger_sql (schema : schema) (table : table) =
  match table.broadcast_channel with
  | None -> None
  | Some (Column column_name) -> (
      match (ready_query_for_table schema table.name, parent_id_column table) with
      | Some query, Some id_column ->
          let function_name = "realtime_notify_" ^ table.name in
          let query_row_sql = replace_first_param (embedded_query_sql query.sql) "row_id" in
          let ready_sql : string =
            Printf.sprintf
               "DROP TRIGGER IF EXISTS %s ON %s;\nDROP FUNCTION IF EXISTS %s();\n\nCREATE OR REPLACE FUNCTION %s()\nRETURNS TRIGGER AS $$\nDECLARE\n  row_id uuid;\n  current_record RECORD;\n  channel_name TEXT;\n  old_channel_name TEXT;\n  payload JSON;\n  payload_data JSONB;\n  old_payload JSON;\n  did_broadcast BOOLEAN := FALSE;\nBEGIN\n  IF TG_OP = 'DELETE' THEN\n    channel_name := OLD.%s::text;\n    payload := json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', 'DELETE');\n    IF channel_name IS NOT NULL THEN\n      PERFORM pg_notify(channel_name, payload::text);\n    END IF;\n    %s\n    RETURN OLD;\n  END IF;\n  IF TG_OP = 'UPDATE' AND OLD.%s::text IS DISTINCT FROM NEW.%s::text THEN\n    old_channel_name := OLD.%s::text;\n    IF old_channel_name IS NOT NULL THEN\n      old_payload := json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', 'DELETE');\n      PERFORM pg_notify(old_channel_name, old_payload::text);\n    END IF;\n  END IF;\n  row_id := NEW.%s;\n  FOR current_record IN\n    %s\n  LOOP\n    did_broadcast := TRUE;\n    channel_name := current_record.%s::text;\n    %s\n    payload := json_build_object('type', 'patch', 'table', %s, 'id', to_jsonb(current_record.%s), 'action', TG_OP, 'data', payload_data);\n    IF channel_name IS NOT NULL THEN\n      PERFORM pg_notify(channel_name, payload::text);\n    END IF;\n    %s\n  END LOOP;\n  IF TG_OP = 'UPDATE' AND NOT did_broadcast THEN\n    channel_name := OLD.%s::text;\n    IF channel_name IS NOT NULL THEN\n      payload := json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', 'DELETE');\n      PERFORM pg_notify(channel_name, payload::text);\n      %s\n    END IF;\n  END IF;\n  RETURN NEW;\nEND;\n$$ LANGUAGE plpgsql;\n\nCREATE TRIGGER %s\nAFTER INSERT OR UPDATE OR DELETE ON %s\nFOR EACH ROW EXECUTE FUNCTION %s();"
                function_name table.name function_name function_name column_name
                (sql_string_literal table.name) (deleted_id_expression table)
                (maybe_touch_premise "channel_name") column_name column_name column_name
                (sql_string_literal table.name) (deleted_id_expression table) id_column
                query_row_sql column_name (query_data_assignment_sql "current_record" query)
                (sql_string_literal table.name) id_column
                (maybe_touch_premise "channel_name") column_name
                (sql_string_literal table.name) (deleted_id_expression table)
                (maybe_touch_premise "channel_name") function_name table.name function_name
          in
          Some ready_sql
      | _ ->
          let function_name = "realtime_notify_" ^ table.name in
          let channel_assignment = Option.value (direct_channel_assignment table) ~default:"channel_name := NULL;" in
          let new_payload : string =
            Printf.sprintf
              "json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', TG_OP, 'data', CASE WHEN TG_OP = 'DELETE' THEN NULL ELSE to_jsonb(NEW) END)"
              (sql_string_literal table.name)
              (Printf.sprintf "CASE WHEN TG_OP = 'DELETE' THEN %s ELSE %s END"
                 (deleted_id_expression table) (current_id_expression table))
          in
          let old_channel_check : string =
            Printf.sprintf
              "IF TG_OP = 'UPDATE' AND OLD.%s::text IS DISTINCT FROM NEW.%s::text THEN\n    old_channel_name := OLD.%s::text;\n    IF old_channel_name IS NOT NULL THEN\n      old_payload := json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', 'DELETE');\n      PERFORM pg_notify(old_channel_name, old_payload::text);\n      channel_name := NEW.%s::text;\n    END IF;\n  END IF;"
              column_name column_name column_name (sql_string_literal table.name)
              (deleted_id_expression table) column_name
          in
          Some
            (String.concat "\n"
               [ "DROP TRIGGER IF EXISTS " ^ function_name ^ " ON " ^ table.name ^ ";";
                 "DROP FUNCTION IF EXISTS " ^ function_name ^ "();";
                 "";
                 "CREATE OR REPLACE FUNCTION " ^ function_name ^ "()";
                 "RETURNS TRIGGER AS $$";
                 "DECLARE";
                 "  channel_name TEXT;";
                 "  old_channel_name TEXT;";
                 "  payload JSON;";
                 "  old_payload JSON;";
                 "BEGIN";
                 "  " ^ channel_assignment;
                 "  " ^ old_channel_check;
                 "  payload := " ^ new_payload ^ ";";
                 "  IF channel_name IS NOT NULL THEN";
                 "    PERFORM pg_notify(channel_name, payload::text);";
                 "  END IF;";
                 "  " ^ (maybe_touch_premise "channel_name");
                 "  RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;";
                 "END;";
                 "$$ LANGUAGE plpgsql;";
                 "";
                 "CREATE TRIGGER " ^ function_name;
                 "AFTER INSERT OR UPDATE OR DELETE ON " ^ table.name;
                 "FOR EACH ROW EXECUTE FUNCTION " ^ function_name ^ "();" ]))
  | Some _ ->
      let function_name = "realtime_notify_" ^ table.name in
      let channel_assignment = Option.value (direct_channel_assignment table) ~default:"channel_name := NULL;" in
      let new_payload =
        Printf.sprintf
          "json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', TG_OP, 'data', CASE WHEN TG_OP = 'DELETE' THEN NULL ELSE to_jsonb(NEW) END)"
          (sql_string_literal table.name)
          (Printf.sprintf "CASE WHEN TG_OP = 'DELETE' THEN %s ELSE %s END"
             (deleted_id_expression table) (current_id_expression table))
      in
      Some
        (String.concat "\n"
           [ "DROP TRIGGER IF EXISTS " ^ function_name ^ " ON " ^ table.name ^ ";";
             "DROP FUNCTION IF EXISTS " ^ function_name ^ "();";
             "";
             "CREATE OR REPLACE FUNCTION " ^ function_name ^ "()";
             "RETURNS TRIGGER AS $$";
             "DECLARE";
             "  channel_name TEXT;";
             "  payload JSON;";
             "BEGIN";
             "  " ^ channel_assignment;
             "  payload := " ^ new_payload ^ ";";
             "  IF channel_name IS NOT NULL THEN";
             "    PERFORM pg_notify(channel_name, payload::text);";
             "  END IF;";
             "  " ^ (maybe_touch_premise "channel_name");
             "  RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;";
             "END;";
              "$$ LANGUAGE plpgsql;";
              "";
              "CREATE TRIGGER " ^ function_name;
              "AFTER INSERT OR UPDATE OR DELETE ON " ^ table.name;
              "FOR EACH ROW EXECUTE FUNCTION " ^ function_name ^ "();" ])

let views_broadcast_trigger_sql (schema : schema) (table : table) =
  match table.broadcast_to_views with
  | None -> None
  | Some config ->
      let function_name = "realtime_notify_" ^ table.name ^ "_views" in
      let query = ready_query_for_table schema table.name in
      match query with
      | Some q ->
          let query_row_sql = replace_first_param (embedded_query_sql q.sql) "NEW.id" in
          Some
            (Printf.sprintf
               "DROP TRIGGER IF EXISTS %s ON %s;\nDROP FUNCTION IF EXISTS %s();\n\nCREATE OR REPLACE FUNCTION %s()\nRETURNS TRIGGER AS $$\nDECLARE\n  view_record RECORD;\n  channel_name TEXT;\n  payload JSON;\n  payload_data JSONB;\n  normalized_record RECORD;\nBEGIN\n  IF TG_OP = 'DELETE' THEN\n    payload := json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', 'DELETE');\n    FOR view_record IN\n      SELECT DISTINCT %s FROM %s\n    LOOP\n      channel_name := view_record.%s::text;\n      IF channel_name IS NOT NULL THEN\n        PERFORM pg_notify(channel_name, payload::text);\n      END IF;\n    END LOOP;\n    RETURN OLD;\n  END IF;\n  FOR normalized_record IN\n    %s\n  LOOP\n    %s\n    payload := json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', TG_OP, 'data', payload_data);\n    FOR view_record IN\n      SELECT DISTINCT %s FROM %s\n    LOOP\n      channel_name := view_record.%s::text;\n      IF channel_name IS NOT NULL THEN\n        PERFORM pg_notify(channel_name, payload::text);\n      END IF;\n    END LOOP;\n  END LOOP;\n  RETURN NEW;\nEND;\n$$ LANGUAGE plpgsql;\n\nCREATE TRIGGER %s\nAFTER INSERT OR UPDATE OR DELETE ON %s\nFOR EACH ROW EXECUTE FUNCTION %s();"
               function_name table.name function_name function_name
               (sql_string_literal table.name) (deleted_id_expression table)
               config.channel_column config.view_table
               config.channel_column
               query_row_sql
               (query_data_assignment_sql "normalized_record" q)
               (sql_string_literal table.name) (current_id_expression table)
               config.channel_column config.view_table
               config.channel_column
               function_name table.name function_name)
      | None ->
          Some
            (Printf.sprintf
               "DROP TRIGGER IF EXISTS %s ON %s;\nDROP FUNCTION IF EXISTS %s();\n\nCREATE OR REPLACE FUNCTION %s()\nRETURNS TRIGGER AS $$\nDECLARE\n  view_record RECORD;\n  channel_name TEXT;\n  payload JSON;\nBEGIN\n  IF TG_OP = 'DELETE' THEN\n    payload := json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', 'DELETE');\n    FOR view_record IN\n      SELECT DISTINCT %s FROM %s\n    LOOP\n      channel_name := view_record.%s::text;\n      IF channel_name IS NOT NULL THEN\n        PERFORM pg_notify(channel_name, payload::text);\n      END IF;\n    END LOOP;\n    RETURN OLD;\n  END IF;\n  payload := json_build_object('type', 'patch', 'table', %s, 'id', %s, 'action', TG_OP, 'data', to_jsonb(NEW));\n  FOR view_record IN\n      SELECT DISTINCT %s FROM %s\n    LOOP\n      channel_name := view_record.%s::text;\n      IF channel_name IS NOT NULL THEN\n        PERFORM pg_notify(channel_name, payload::text);\n      END IF;\n    END LOOP;\n  RETURN NEW;\nEND;\n$$ LANGUAGE plpgsql;\n\nCREATE TRIGGER %s\nAFTER INSERT OR UPDATE OR DELETE ON %s\nFOR EACH ROW EXECUTE FUNCTION %s();"
               function_name table.name function_name function_name
               (sql_string_literal table.name) (deleted_id_expression table)
               config.channel_column config.view_table
               config.channel_column
               (sql_string_literal table.name) (current_id_expression table)
               config.channel_column config.view_table
               config.channel_column
               function_name table.name function_name)

let parent_fk_column (child_table : table) parent_table =
  child_table.columns
  |> List.find_map (fun (column : column) ->
         match column.foreign_key with
         | Some foreign_key when foreign_key.referenced_table = parent_table -> Some column.name
         | _ -> None)

let table_has_column (table : table) column_name = column_by_name table column_name <> None

let parent_query_sql (schema : schema) query_name =
  schema.queries |> List.find_opt (fun (query : query) -> query.name = query_name)

let parent_channel_from_query (parent_table : table) parent_query_sql =
  match parent_table.broadcast_channel with
  | Some (Column column_name) -> Printf.sprintf "parent_record.%s::text" column_name
  | Some (Computed expression) | Some (Conditional expression) ->
      Printf.sprintf
        "(SELECT (%s)::text FROM (%s) AS computed_parent LIMIT 1)"
        expression parent_query_sql
  | Some (Subquery query_sql) ->
      Printf.sprintf
        "(SELECT generated_channel.channel_name::text FROM (%s) AS generated_channel(channel_name) LIMIT 1)"
        query_sql
  | None -> "NULL"

let parent_touch_sql (parent_table : table) =
  if table_has_column parent_table "premise_id" then maybe_touch_premise "parent_record.premise_id::text" else ""

let parent_broadcast_trigger_sql (schema : schema) (child_table : table) =
  match child_table.broadcast_parent with
  | None -> None
  | Some parent when parent.parent_table = child_table.name -> None
  | Some parent ->
      let parent_table =
        List.find_opt (fun (table : table) -> table.name = parent.parent_table) schema.tables
      in
      let parent_query = parent_query_sql schema parent.query_name in
      let parent_fk = parent_fk_column child_table parent.parent_table in
      (match (parent_table, parent_query, parent_fk) with
      | Some parent_table, Some query, Some parent_fk -> (
          match parent_id_column parent_table with
           | None -> None
           | Some parent_id_column ->
               let function_name = "realtime_notify_" ^ child_table.name ^ "_parent" in
               let parent_row_sql = replace_first_param (embedded_query_sql query.sql) "parent_row_id" in
               Some
                  (Printf.sprintf
                     "DROP TRIGGER IF EXISTS %s ON %s;\nDROP FUNCTION IF EXISTS %s();\n\nCREATE OR REPLACE FUNCTION %s()\nRETURNS TRIGGER AS $$\nDECLARE\n  parent_row_id uuid;\n  parent_record RECORD;\n  channel_name TEXT;\n  payload JSON;\n  payload_data JSONB;\n  did_broadcast BOOLEAN := FALSE;\nBEGIN\n  parent_row_id := CASE WHEN TG_OP = 'DELETE' THEN OLD.%s ELSE NEW.%s END;\n  IF parent_row_id IS NULL THEN\n    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;\n  END IF;\n  FOR parent_record IN\n    %s\n  LOOP\n    did_broadcast := TRUE;\n    channel_name := %s;\n    %s\n    payload := json_build_object('type', 'patch', 'table', %s, 'id', to_jsonb(parent_record.%s), 'action', 'UPDATE', 'data', payload_data);\n    IF channel_name IS NOT NULL THEN\n      PERFORM pg_notify(channel_name, payload::text);\n    END IF;\n    %s\n  END LOOP;\n  IF NOT did_broadcast THEN\n    %s\n    IF channel_name IS NOT NULL THEN\n      payload := json_build_object('type', 'patch', 'table', %s, 'id', to_jsonb(parent_row_id), 'action', 'DELETE');\n      PERFORM pg_notify(channel_name, payload::text);\n      %s\n    END IF;\n  END IF;\n  RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;\nEND;\n$$ LANGUAGE plpgsql;\n\nCREATE TRIGGER %s\nAFTER INSERT OR UPDATE OR DELETE ON %s\nFOR EACH ROW EXECUTE FUNCTION %s();"
                     function_name child_table.name function_name function_name parent_fk parent_fk
                     parent_row_sql (parent_channel_from_query parent_table parent_row_sql)
                     (query_data_assignment_sql "parent_record" query)
                     (sql_string_literal parent.parent_table) parent_id_column
                     (parent_touch_sql parent_table)
                     (delete_channel_lookup_sql parent_table)
                     (sql_string_literal parent.parent_table)
                     (maybe_touch_premise "channel_name")
                    function_name child_table.name function_name))
      | _ -> None)

let reverse_parent_triggers_sql (schema : schema) (source_table : table) =
  schema.tables
  |> list_filter_map (fun (child_table : table) ->
         match child_table.broadcast_parent with
          | None -> None
          | Some parent when parent.parent_table = source_table.name -> None
          | Some parent ->
             let parent_table =
               List.find_opt (fun (table : table) -> table.name = parent.parent_table) schema.tables
             in
             let parent_query = parent_query_sql schema parent.query_name in
             let parent_fk = parent_fk_column child_table parent.parent_table in
             (match (parent_table, parent_query, parent_fk, source_table.id_column) with
             | Some parent_table, Some query, Some parent_fk, Some source_id_column -> (
                 match parent_id_column parent_table with
                 | None -> None
                 | Some parent_id_column ->
                     let bridge_columns =
                       child_table.columns
                       |> List.filter_map (fun (column : column) ->
                              match column.foreign_key with
                              | Some foreign_key when foreign_key.referenced_table = source_table.name -> Some column.name
                              | _ -> None)
                     in
                     if bridge_columns = [] then
                       None
                     else
                       let function_name =
                         Printf.sprintf "realtime_notify_%s_via_%s" source_table.name child_table.name
                       in
                       let source_id_expr =
                         Printf.sprintf "CASE WHEN TG_OP = 'DELETE' THEN OLD.%s ELSE NEW.%s END" source_id_column source_id_column
                       in
                       let bridge_filter =
                         bridge_columns
                         |> List.map (fun column_name -> Printf.sprintf "%s = source_row_id" column_name)
                         |> String.concat " OR "
                       in
                       let parent_row_sql = replace_first_param (embedded_query_sql query.sql) "parent_row_id" in
                       Some
                         (Printf.sprintf
                            "DROP TRIGGER IF EXISTS %s ON %s;\nDROP FUNCTION IF EXISTS %s();\n\nCREATE OR REPLACE FUNCTION %s()\nRETURNS TRIGGER AS $$\nDECLARE\n  source_row_id uuid;\n  parent_row_id uuid;\n  parent_record RECORD;\n  channel_name TEXT;\n  payload JSON;\n  payload_data JSONB;\n  did_broadcast BOOLEAN;\nBEGIN\n  source_row_id := %s;\n  IF source_row_id IS NULL THEN\n    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;\n  END IF;\n  FOR parent_row_id IN\n    SELECT DISTINCT %s FROM %s WHERE %s\n  LOOP\n    did_broadcast := FALSE;\n    FOR parent_record IN\n      %s\n    LOOP\n      did_broadcast := TRUE;\n      channel_name := %s;\n      %s\n      payload := json_build_object('type', 'patch', 'table', %s, 'id', to_jsonb(parent_record.%s), 'action', 'UPDATE', 'data', payload_data);\n      IF channel_name IS NOT NULL THEN\n        PERFORM pg_notify(channel_name, payload::text);\n      END IF;\n      %s\n    END LOOP;\n    IF NOT did_broadcast THEN\n      %s\n      IF channel_name IS NOT NULL THEN\n        payload := json_build_object('type', 'patch', 'table', %s, 'id', to_jsonb(parent_row_id), 'action', 'DELETE');\n        PERFORM pg_notify(channel_name, payload::text);\n        %s\n      END IF;\n    END IF;\n  END LOOP;\n  RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;\nEND;\n$$ LANGUAGE plpgsql;\n\nCREATE TRIGGER %s\nAFTER INSERT OR UPDATE OR DELETE ON %s\nFOR EACH ROW EXECUTE FUNCTION %s();"
                            function_name source_table.name function_name function_name source_id_expr parent_fk
                            child_table.name bridge_filter parent_row_sql
                            (parent_channel_from_query parent_table parent_row_sql)
                            (query_data_assignment_sql "parent_record" query)
                            (sql_string_literal parent.parent_table)
                            parent_id_column
                            (parent_touch_sql parent_table)
                            (delete_channel_lookup_sql parent_table)
                            (sql_string_literal parent.parent_table)
                            (maybe_touch_premise "channel_name")
                            function_name source_table.name function_name))
             | _ -> None))

let generated_triggers_sql (schema : schema) =
  let parts =
    schema.tables
    |> List.concat_map (fun table ->
            let direct = Option.to_list (direct_trigger_sql schema table) in
            let views = Option.to_list (views_broadcast_trigger_sql schema table) in
            let parent = Option.to_list (parent_broadcast_trigger_sql schema table) in
            let reverse = reverse_parent_triggers_sql schema table in
            direct @ views @ parent @ reverse)
  in
  String.concat "\n\n" parts

let mutation_action_table_sql (mutation : mutation) =
  Printf.sprintf
    "CREATE TABLE IF NOT EXISTS _resync_actions_%s (\n\
    \  action_id uuid PRIMARY KEY,\n\
    \  status text NOT NULL CHECK (status IN ('ok', 'failed')),\n\
    \  processed_at timestamptz NOT NULL DEFAULT NOW(),\n\
    \  error_message text\n\
    );"
    mutation.name

let mutation_action_tables_sql (schema : schema) =
  schema.mutations
  |> List.map mutation_action_table_sql
  |> String.concat "\n\n"

let migration_version () =
  let tm = Unix.gmtime (Unix.time ()) in
  Printf.sprintf "%04d%02d%02d%02d%02d%02d"
    (tm.Unix.tm_year + 1900) (tm.Unix.tm_mon + 1) tm.Unix.tm_mday tm.Unix.tm_hour
    tm.Unix.tm_min tm.Unix.tm_sec

let snapshot_to_yojson (snapshot : snapshot) =
  let column_to_yojson (column : column) =
    `Assoc
      [ ("name", `String column.name);
        ("sql_type", `String column.sql_type_raw);
        ("nullable", `Bool column.nullable);
        ("primary_key", `Bool column.primary_key);
        ("default", option_map `Null (fun value -> `String value) column.default);
        ( "foreign_key",
          match column.foreign_key with
          | None -> `Null
          | Some foreign_key ->
              `Assoc
                [ ("column", `String foreign_key.column);
                  ("referenced_table", `String foreign_key.referenced_table);
                  ("referenced_column", `String foreign_key.referenced_column) ] ) ]
  in
  let table_to_yojson (table : table_snapshot) =
    `Assoc
      [ ("table_name", `String table.table_name);
        ("columns", `List (List.map column_to_yojson table.columns));
        ("id_column", option_map `Null (fun value -> `String value) table.id_column);
        ("composite_key", `List (List.map (fun value -> `String value) table.composite_key)) ]
  in
  `Assoc
    [ ("schema_hash", `String snapshot.schema_hash);
      ("tables", `List (List.map table_to_yojson snapshot.tables)) ]

let snapshot_of_yojson json =
  let open Yojson.Safe.Util in
  let column_of_yojson json =
    let foreign_key =
      match json |> member "foreign_key" with
      | `Null -> None
      | foreign_key_json ->
          Some
            {
              column = foreign_key_json |> member "column" |> to_string;
              referenced_table = foreign_key_json |> member "referenced_table" |> to_string;
              referenced_column = foreign_key_json |> member "referenced_column" |> to_string;
            }
    in
    {
      name = json |> member "name" |> to_string;
      sql_type = json |> member "sql_type" |> to_string |> sql_type_of_string;
      sql_type_raw = json |> member "sql_type" |> to_string;
      nullable = json |> member "nullable" |> to_bool;
      primary_key = json |> member "primary_key" |> to_bool;
      default = (match json |> member "default" with `Null -> None | value -> Some (to_string value));
      foreign_key;
      definition_sql = "";
    }
  in
  let table_of_yojson json =
    {
      table_name = json |> member "table_name" |> to_string;
      columns = json |> member "columns" |> to_list |> List.map column_of_yojson;
      id_column = (match json |> member "id_column" with `Null -> None | value -> Some (to_string value));
      composite_key = json |> member "composite_key" |> to_list |> List.map to_string;
    }
  in
  {
    schema_hash = json |> member "schema_hash" |> to_string;
    tables = json |> member "tables" |> to_list |> List.map table_of_yojson;
  }

let column_signature (column : column) =
  ( column.sql_type_raw,
    column.nullable,
    column.primary_key,
    column.default,
    Option.map (fun foreign_key -> (foreign_key.referenced_table, foreign_key.referenced_column)) column.foreign_key )

let fk_constraint_name table_name column_name = Printf.sprintf "%s_%s_fkey" table_name column_name

let alter_statements_for_column ~table_name old_column new_column =
  let statements = ref [] in
  if old_column.sql_type_raw <> new_column.sql_type_raw then
    statements :=
      Printf.sprintf "ALTER TABLE %s ALTER COLUMN %s TYPE %s;" table_name new_column.name new_column.sql_type_raw
      :: !statements;
  if old_column.nullable <> new_column.nullable then
    statements :=
      Printf.sprintf "ALTER TABLE %s ALTER COLUMN %s %s NOT NULL;" table_name new_column.name
        (if new_column.nullable then "DROP" else "SET")
      :: !statements;
  if old_column.default <> new_column.default then
    statements :=
      (match new_column.default with
      | Some default ->
          Printf.sprintf "ALTER TABLE %s ALTER COLUMN %s SET DEFAULT %s;" table_name new_column.name default
      | None ->
          Printf.sprintf "ALTER TABLE %s ALTER COLUMN %s DROP DEFAULT;" table_name new_column.name)
      :: !statements;
  let old_fk = Option.map (fun foreign_key -> (foreign_key.referenced_table, foreign_key.referenced_column)) old_column.foreign_key in
  let new_fk = Option.map (fun foreign_key -> (foreign_key.referenced_table, foreign_key.referenced_column)) new_column.foreign_key in
  if old_fk <> new_fk then (
    statements :=
      Printf.sprintf "ALTER TABLE %s DROP CONSTRAINT IF EXISTS %s;" table_name (fk_constraint_name table_name new_column.name)
      :: !statements;
    match new_column.foreign_key with
    | Some foreign_key ->
        statements :=
          Printf.sprintf
            "ALTER TABLE %s ADD CONSTRAINT %s FOREIGN KEY (%s) REFERENCES %s(%s);"
            table_name (fk_constraint_name table_name new_column.name) new_column.name
            foreign_key.referenced_table foreign_key.referenced_column
          :: !statements
    | None -> ());
  List.rev !statements

let diff_tables ~(old_snapshot : snapshot) ~(new_schema : schema) =
  let old_by_name = Hashtbl.create (List.length old_snapshot.tables) in
  List.iter (fun (table : table_snapshot) -> Hashtbl.replace old_by_name table.table_name table) old_snapshot.tables;
  let new_by_name = Hashtbl.create (List.length new_schema.tables) in
  List.iter (fun (table : table) -> Hashtbl.replace new_by_name table.name table) new_schema.tables;
  let statements = ref [] in
  List.iter
    (fun (table : table_snapshot) ->
      if not (Hashtbl.mem new_by_name table.table_name) then
        statements := Printf.sprintf "DROP TABLE IF EXISTS %s CASCADE;" table.table_name :: !statements)
    old_snapshot.tables;
  List.iter
    (fun (table : table) ->
      match Hashtbl.find_opt old_by_name table.name with
      | None -> statements := table.create_sql :: !statements
      | Some old_table ->
          let old_columns = Hashtbl.create (List.length old_table.columns) in
          List.iter (fun (column : column) -> Hashtbl.replace old_columns column.name column) old_table.columns;
          let new_columns = Hashtbl.create (List.length table.columns) in
          List.iter (fun (column : column) -> Hashtbl.replace new_columns column.name column) table.columns;
          List.iter
            (fun (column : column) ->
              if not (Hashtbl.mem new_columns column.name) then
                statements :=
                  Printf.sprintf "ALTER TABLE %s DROP COLUMN IF EXISTS %s CASCADE;" table.name column.name
                  :: !statements)
            old_table.columns;
          List.iter
            (fun (column : column) ->
              match Hashtbl.find_opt old_columns column.name with
              | None ->
                  statements :=
                    Printf.sprintf "ALTER TABLE %s ADD COLUMN %s;" table.name column.definition_sql :: !statements
              | Some old_column ->
                  statements := List.rev_append (alter_statements_for_column ~table_name:table.name old_column column) !statements)
            table.columns;
          if old_table.composite_key <> table.composite_key && table.composite_key <> [] then
            statements :=
              Printf.sprintf "ALTER TABLE %s DROP CONSTRAINT IF EXISTS %s_pkey;" table.name table.name
              :: Printf.sprintf "ALTER TABLE %s ADD PRIMARY KEY (%s);" table.name (String.concat ", " table.composite_key)
              :: !statements)
    new_schema.tables;
  List.rev !statements

let migration_sql ?previous_snapshot (schema : schema) =
  let version = migration_version () in
  let statements =
    match previous_snapshot with
    | None -> List.map (fun (table : table) -> table.create_sql) schema.tables
    | Some snapshot -> diff_tables ~old_snapshot:snapshot ~new_schema:schema
  in
  let statements =
    statements
    @ [
        mutation_action_tables_sql schema;
        "CREATE TABLE IF NOT EXISTS schema_migrations (version varchar PRIMARY KEY, applied_at timestamp NOT NULL DEFAULT NOW());";
        generated_triggers_sql schema;
        Printf.sprintf "INSERT INTO schema_migrations (version) VALUES (%S) ON CONFLICT (version) DO NOTHING;" version;
      ]
  in
  (version, String.concat "\n\n" statements ^ "\n")

let table_caqti_type_declaration (table : table) =
  let columns_list : column list = table.columns in
  match columns_list with
  | [] -> "let caqti_type = Caqti_type.unit [@@platform native]"
  | _ ->
      let field_names = List.map (fun (c : column) -> sanitize_identifier c.name) columns_list in
      let constructor_args = String.concat " " field_names in
      let record_body =
        field_names
        |> List.map (fun name -> Printf.sprintf "%s = %s;" name name)
        |> String.concat " "
      in
      let proj_chain =
        let record_type = type_name_of_table table.name in
        columns_list
        |> List.map (fun (col : column) ->
               Printf.sprintf "    @@ Caqti_type.proj %s (fun (r : %s) -> r.%s)"
                 (caqti_type_constructor_of_sql_type col.sql_type)
                 record_type
                 (sanitize_identifier col.name))
        |> String.concat "\n"
      in
      Printf.sprintf
        "let caqti_type =\n    Caqti_type.product(fun %s -> { %s })\n%s\n    @@ Caqti_type.proj_end [@@platform native]"
        constructor_args record_body proj_chain

let table_module_declaration (table : table) =
  Printf.sprintf
    "module %s = struct\n  let name = %S\n  let id_column = %s\n  let composite_key = %s\n\n  %s\nend"
    (module_name_of_table table.name) table.name
    (match table.id_column with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    (string_list_literal table.composite_key)
    (table_caqti_type_declaration table)

let dispatch_mutation_source (mutations : mutation list) =
  let cases =
    mutations
    |> List.filter (fun (m : mutation) -> m.handler = Sql)
    |> List.map (fun (m : mutation) ->
           Printf.sprintf "  | %S -> Some (Mutations.%s.dispatch (module Db) action)"
             m.name (module_name_of_table m.name))
    |> String.concat "\n"
  in
  Printf.sprintf
    "let dispatch_mutation (module Db : Caqti_lwt.CONNECTION) ~mutation_name action =\n\
    \  match mutation_name with\n\
    %s\n\
    \  | _ -> None\n\
    [@@platform native]"
    cases

let module_source (schema : schema) =
  let table_types = schema.tables |> List.map record_type_declaration |> String.concat "\n\n" in
  let table_values = schema.tables |> List.map table_record_literal |> String.concat ";\n  " in
  let query_values = schema.queries |> List.map query_record_literal |> String.concat ";\n  " in
  let mutation_values = schema.mutations |> List.map mutation_record_literal |> String.concat ";\n  " in
  let query_modules = schema.queries |> List.map (query_module_declaration schema.tables) |> String.concat "\n\n" in
  let mutation_modules = schema.mutations |> List.map mutation_module_declaration |> String.concat "\n\n" in
  let table_modules =
    schema.tables
    |> List.map table_module_declaration
    |> String.concat "\n\n"
  in
  let _, migration_sql = migration_sql schema in
  Printf.sprintf
    "include struct\n\ntype sql_type =\n  | Uuid\n  | Varchar\n  | Text\n  | Int\n  | Bigint\n  | Boolean\n  | Timestamp\n  | Timestamptz\n  | Json\n  | Jsonb\n  | Custom of string\n\ntype foreign_key = {\n  column : string;\n  referenced_table : string;\n  referenced_column : string;\n}\n\ntype broadcast_channel =\n  | Column of string\n  | Computed of string\n  | Conditional of string\n  | Subquery of string\n\ntype broadcast_parent = {\n  parent_table : string;\n  query_name : string;\n}\n\ntype broadcast_to_views = {\n  view_table : string;\n  channel_column : string;\n}\n\ntype column_metadata = {\n  name : string;\n  sql_type : sql_type;\n  sql_type_raw : string;\n  nullable : bool;\n  primary_key : bool;\n  default : string option;\n  foreign_key : foreign_key option;\n  definition_sql : string;\n}\n\ntype table_metadata = {\n  name : string;\n  columns : column_metadata list;\n  id_column : string option;\n  composite_key : string list;\n  broadcast_channel : broadcast_channel option;\n  broadcast_parent : broadcast_parent option;\n  broadcast_to_views : broadcast_to_views option;\n  create_sql : string;\n  source_file : string;\n}\n\ntype handler = Sql | Ocaml\n\ntype query_param = {\n  index : int;\n  column_ref : (string * string) option;
  payload_key : string option;\n  ocaml_type : string;\n  sql_type : string;\n}\n\ntype query_metadata = {\n  name : string;\n  sql : string;\n  source_file : string;\n  cache_key : string option;\n  return_table : string option;\n  json_columns : string list;\n  params_metadata : query_param list;\n handler : handler;\n}\n\ntype mutation_metadata = {\n name : string;\n sql : string;\n source_file : string;\n params_metadata : query_param list;\n  handler : handler;\n}\n\n%s\n\nlet schema_hash = %S\n\nlet source_files = %s\n\nlet tables : table_metadata list = [\n  %s\n]\n\nlet queries : query_metadata list = [\n  %s\n]\n\nlet mutations : mutation_metadata list = [\n  %s\n]\n\nlet generated_triggers_sql = %S\n\nlet latest_migration_sql = %S\n\nlet find_query name = List.find_opt (fun (query : query_metadata) -> query.name = name) queries\n\nlet find_mutation name = List.find_opt (fun (mutation : mutation_metadata) -> mutation.name = name) mutations\n\nlet find_table name = List.find_opt (fun (table : table_metadata) -> table.name = name) tables\n\nlet find_column table_name column_name =\n  match find_table table_name with\n  | Some table -> List.find_opt (fun (column : column_metadata) -> column.name = column_name) table.columns\n  | None -> None\n\nlet table_name name = match find_table name with Some table -> table.name | None -> name\n\nmodule Tables = struct\n%s\nend\n\nmodule Queries = struct\n%s\nend\n\nmodule Mutations = struct\n%s\nend\n\n%s\nend"
    table_types schema.schema_hash (string_list_literal schema.source_files)
    (indent table_values) (indent query_values) (indent mutation_values)
    (generated_triggers_sql schema) migration_sql
    (indent table_modules) (indent query_modules) (indent mutation_modules)
    (dispatch_mutation_source schema.mutations)

let write_file path contents =
  let channel = open_out_bin path in
  Fun.protect ~finally:(fun () -> close_out channel) (fun () -> output_string channel contents)

let ensure_directory path =
  if Sys.file_exists path then
    ()
  else
    Unix.mkdir path 0o755

let write_generated_artifacts ~sql_dir ~triggers_path ~snapshot_path ~migrations_dir =
  let schema = Realtime_schema_parser.parse_directory sql_dir in
  ensure_directory (Filename.dirname triggers_path);
  ensure_directory (Filename.dirname snapshot_path);
  ensure_directory migrations_dir;
  write_file triggers_path (generated_triggers_sql schema ^ "\n");
  let snapshot = snapshot_of_schema schema in
  let previous_snapshot =
    if Sys.file_exists snapshot_path then
      Some (snapshot_of_yojson (Yojson.Safe.from_file snapshot_path))
    else
      None
  in
  let should_write_migration =
    match previous_snapshot with
    | Some previous -> previous.schema_hash <> snapshot.schema_hash
    | None -> true
  in
  if should_write_migration then (
    let version, migration = migration_sql ?previous_snapshot schema in
    let migration_path = Filename.concat migrations_dir (Printf.sprintf "migration_%s_schema.sql" version) in
    write_file migration_path migration;
    write_file snapshot_path (Yojson.Safe.pretty_to_string (snapshot_to_yojson snapshot) ^ "\n"))
  else if not (Sys.file_exists snapshot_path) then
    write_file snapshot_path (Yojson.Safe.pretty_to_string (snapshot_to_yojson snapshot) ^ "\n");
  schema
