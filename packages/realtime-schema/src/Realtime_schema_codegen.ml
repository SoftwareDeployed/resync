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
    "{ name = %S; columns = %s; id_column = %s; composite_key = %s; broadcast_channel = %s; broadcast_parent = %s; create_sql = %S; source_file = %S }"
    table.name columns_literal
    (match table.id_column with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    (string_list_literal table.composite_key)
    (ocaml_of_broadcast_channel table.broadcast_channel)
    (ocaml_of_broadcast_parent table.broadcast_parent)
    table.create_sql table.source_file

let query_record_literal (query : query) =
  let params_literal =
    query.params
    |> List.map (fun param ->
           Printf.sprintf
             "{ index = %d; column_ref = %s; ocaml_type = %S; sql_type = %S }"
             param.index
             (match param.column_ref with
             | None -> "None"
             | Some (table_name, column_name) ->
                 Printf.sprintf "Some (%S, %S)" table_name column_name)
             param.ocaml_type param.sql_type)
    |> String.concat "; "
    |> Printf.sprintf "[%s]"
  in
  let json_columns_literal = string_list_literal query.json_columns in
  Printf.sprintf
    "{ name = %S; sql = %S; source_file = %S; cache_key = %s; return_table = %s; json_columns = %s; params = %s }"
    query.name query.sql query.source_file
    (match query.cache_key with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    (match query.return_table with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    json_columns_literal params_literal

let column_type_annotation (column : column) =
  match ocaml_type_of_sql_type column.sql_type with
  | "string" -> "string"
  | "int" -> "int"
  | "int64" -> "int64"
  | "bool" -> "bool"
  | "float" -> "float"
  | "Yojson.Safe.t" -> "Yojson.Safe.t"
  | other -> other

let record_type_declaration (table : table) =
  let fields =
    table.columns
    |> List.map (fun (column : column) -> Printf.sprintf "  %s : %s;" (sanitize_identifier column.name) (column_type_annotation column))
    |> String.concat "\n"
  in
  Printf.sprintf "type %s = {\n%s\n}" (type_name_of_table table.name) fields

let query_module_declaration (query : query) =
  let params_literal =
    query.params
    |> List.map (fun param -> Printf.sprintf "(%d, %S, %S)" param.index param.ocaml_type param.sql_type)
    |> String.concat "; "
  in
  Printf.sprintf
    "module %s = struct\n  let name = %S\n  let sql = %S\n  let params = [%s]\n  let return_table = %s\n  let json_columns = %s\nend"
    (module_name_of_table query.name) query.name query.sql params_literal
    (match query.return_table with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
    (string_list_literal query.json_columns)

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
            let parent = Option.to_list (parent_broadcast_trigger_sql schema table) in
            let reverse = reverse_parent_triggers_sql schema table in
            direct @ parent @ reverse)
  in
  String.concat "\n\n" parts

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
        "CREATE TABLE IF NOT EXISTS schema_migrations (version varchar PRIMARY KEY, applied_at timestamp NOT NULL DEFAULT NOW());";
        generated_triggers_sql schema;
        Printf.sprintf "INSERT INTO schema_migrations (version) VALUES (%S) ON CONFLICT (version) DO NOTHING;" version;
      ]
  in
  (version, String.concat "\n\n" statements ^ "\n")

let module_source (schema : schema) =
  let table_types = schema.tables |> List.map record_type_declaration |> String.concat "\n\n" in
  let table_values = schema.tables |> List.map table_record_literal |> String.concat ";\n  " in
  let query_values = schema.queries |> List.map query_record_literal |> String.concat ";\n  " in
  let query_modules = schema.queries |> List.map query_module_declaration |> String.concat "\n\n" in
  let table_modules =
    schema.tables
    |> List.map (fun (table : table) ->
           Printf.sprintf
             "module %s = struct\n  let name = %S\n  let id_column = %s\n  let composite_key = %s\nend"
             (module_name_of_table table.name) table.name
             (match table.id_column with None -> "None" | Some value -> Printf.sprintf "Some %S" value)
             (string_list_literal table.composite_key))
    |> String.concat "\n\n"
  in
  let _, migration_sql = migration_sql schema in
  Printf.sprintf
    "include struct\n\ntype sql_type =\n  | Uuid\n  | Varchar\n  | Text\n  | Int\n  | Bigint\n  | Boolean\n  | Timestamp\n  | Timestamptz\n  | Json\n  | Jsonb\n  | Custom of string\n\ntype foreign_key = {\n  column : string;\n  referenced_table : string;\n  referenced_column : string;\n}\n\ntype broadcast_channel =\n  | Column of string\n  | Computed of string\n  | Conditional of string\n  | Subquery of string\n\ntype broadcast_parent = {\n  parent_table : string;\n  query_name : string;\n}\n\ntype column_metadata = {\n  name : string;\n  sql_type : sql_type;\n  sql_type_raw : string;\n  nullable : bool;\n  primary_key : bool;\n  default : string option;\n  foreign_key : foreign_key option;\n  definition_sql : string;\n}\n\ntype table_metadata = {\n  name : string;\n  columns : column_metadata list;\n  id_column : string option;\n  composite_key : string list;\n  broadcast_channel : broadcast_channel option;\n  broadcast_parent : broadcast_parent option;\n  create_sql : string;\n  source_file : string;\n}\n\ntype query_param = {\n  index : int;\n  column_ref : (string * string) option;\n  ocaml_type : string;\n  sql_type : string;\n}\n\ntype query_metadata = {\n  name : string;\n  sql : string;\n  source_file : string;\n  cache_key : string option;\n  return_table : string option;\n  json_columns : string list;\n  params : query_param list;\n}\n\n%s\n\nlet schema_hash = %S\n\nlet source_files = %s\n\nlet tables : table_metadata list = [\n  %s\n]\n\nlet queries : query_metadata list = [\n  %s\n]\n\nlet generated_triggers_sql = %S\n\nlet latest_migration_sql = %S\n\nlet find_query name = List.find_opt (fun (query : query_metadata) -> query.name = name) queries\n\nlet find_table name = List.find_opt (fun (table : table_metadata) -> table.name = name) tables\n\nlet find_column table_name column_name =\n  match find_table table_name with\n  | Some table -> List.find_opt (fun (column : column_metadata) -> column.name = column_name) table.columns\n  | None -> None\n\nlet table_name name = match find_table name with Some table -> table.name | None -> name\n\nmodule Tables = struct\n%s\nend\n\nmodule Queries = struct\n%s\nend\nend"
    table_types schema.schema_hash (string_list_literal schema.source_files)
    (indent table_values) (indent query_values) (generated_triggers_sql schema)
    migration_sql (indent table_modules) (indent query_modules)

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
