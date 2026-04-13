open Realtime_schema_types

let get_string_field (fields : (string * Yojson.Basic.t) list) key =
  match List.assoc_opt key fields with
  | Some (`String s) -> Some s
  | Some (`Assoc inner) -> (
      match List.assoc_opt "str" inner with
      | Some (`String s) -> Some s
      | _ -> None)
  | _ -> None

let rec find_nodes
    (predicate : Yojson.Basic.t -> bool)
    (path : (string * Yojson.Basic.t) list)
    (json : Yojson.Basic.t)
    acc =
  match json with
  | `Assoc fields ->
      let acc = if predicate json then (List.rev path, json) :: acc else acc in
      List.fold_left
        (fun acc (key, value) -> find_nodes predicate ((key, value) :: path) value acc)
        acc fields
  | `List items ->
      List.fold_left
        (fun acc (idx, value) -> find_nodes predicate ((string_of_int idx, value) :: path) value acc)
        acc (List.mapi (fun i v -> (i, v)) items)
  | _ -> acc

let find_param_refs (json : Yojson.Basic.t) =
  let is_param_ref = function
    | `Assoc fields when List.mem_assoc "ParamRef" fields -> true
    | _ -> false
  in
  find_nodes is_param_ref [] json []
  |> List.filter_map (fun (path, node) ->
       match node with
       | `Assoc fields -> (
           match List.assoc_opt "ParamRef" fields with
           | Some (`Assoc pfields) -> (
               match get_string_field pfields "number" with
               | Some num_str -> (
                   try
                     let index = int_of_string num_str in
                     Some (index, path)
                   with Failure _ -> None)
               | None -> None)
           | _ -> None)
       | _ -> None)

let find_field_in_path path key =
  let rec loop = function
    | [] -> None
    | (_, `Assoc fields) :: rest -> (
        match List.assoc_opt key fields with
        | Some (value : Yojson.Basic.t) -> Some value
        | None -> loop rest)
    | _ :: rest -> loop rest
  in
  loop path

let find_parent_named path name =
  let rec loop = function
    | [] -> None
    | (_, `Assoc fields) :: rest when List.mem_assoc name fields -> Some fields
    | _ :: rest -> loop rest
  in
  loop path

let get_relation_name (stmt_fields : (string * Yojson.Basic.t) list) =
  match List.assoc_opt "relation" stmt_fields with
  | Some (`Assoc rel_fields) -> (
      match List.assoc_opt "RangeVar" rel_fields with
      | Some (`Assoc rv_fields) -> get_string_field rv_fields "relname"
      | _ -> None)
  | _ -> None

let get_insert_columns (stmt_fields : (string * Yojson.Basic.t) list) =
  match List.assoc_opt "cols" stmt_fields with
  | Some (`List cols) ->
      List.filter_map
        (fun (col : Yojson.Basic.t) ->
          match col with
          | `Assoc cfields -> (
              match List.assoc_opt "ResTarget" cfields with
              | Some (`Assoc rt_fields) -> get_string_field rt_fields "name"
              | _ -> None)
          | _ -> None)
        cols
  | _ -> []

let rec find_param_refs_in_json (json : Yojson.Basic.t) =
  find_param_refs json

let rec find_all_param_refs_in_array (json : Yojson.Basic.t) =
  match json with
  | `List items ->
      List.mapi
        (fun idx item ->
          find_param_refs_in_json item
          |> List.map (fun (n, path) -> (n, idx, path)))
        items
      |> List.concat
  | _ -> []

let resolve_insert tables (stmt_fields : (string * Yojson.Basic.t) list) =
  let table_name = Option.value (get_relation_name stmt_fields) ~default:"" in
  let cols = get_insert_columns stmt_fields in
  let values_lists : Yojson.Basic.t =
    match List.assoc_opt "selectStmt" stmt_fields with
    | Some (`Assoc ss_fields) -> (
        match List.assoc_opt "SelectStmt" ss_fields with
        | Some (`Assoc sel_fields) -> (
            match List.assoc_opt "valuesLists" sel_fields with
            | Some lists -> lists
            | None -> `List [])
        | _ -> `List [])
    | _ -> `List []
  in
  let params = Hashtbl.create 4 in
  (match values_lists with
  | `List rows ->
      List.iteri
        (fun _row_idx row ->
          match row with
          | `List cells ->
              List.iteri
                (fun col_idx cell ->
                  let refs = find_param_refs_in_json cell in
                  List.iter
                    (fun (param_num, _path) ->
                      let column_name =
                        if col_idx < List.length cols then List.nth cols col_idx else ""
                      in
                      let ocaml_type, sql_type =
                        match List.find_opt (fun (t : table) -> t.name = table_name) tables with
                        | Some table -> (
                            match Realtime_schema_utils.column_by_name table column_name with
                            | Some col -> (Realtime_schema_utils.ocaml_type_of_sql_type col.sql_type, col.sql_type_raw)
                            | None -> ("string", "text"))
                        | None -> ("string", "text")
                      in
                      Hashtbl.replace params param_num
                        { index = param_num; column_ref = Some (table_name, column_name); ocaml_type; sql_type })
                    refs)
                cells
          | _ -> ())
        rows
  | _ -> ());
  params

let get_target_list (stmt_fields : (string * Yojson.Basic.t) list) =
  match List.assoc_opt "targetList" stmt_fields with
  | Some (`List items) -> items
  | _ -> []

let get_where_clause (stmt_fields : (string * Yojson.Basic.t) list) =
  match List.assoc_opt "whereClause" stmt_fields with
  | Some wc -> Some wc
  | None -> None

let rec extract_column_ref_from_expr (expr : Yojson.Basic.t) =
  match expr with
  | `Assoc fields -> (
      match List.assoc_opt "ColumnRef" fields with
      | Some (`Assoc cr_fields) -> (
          match List.assoc_opt "fields" cr_fields with
          | Some (`List fields_list) -> (
              let names =
                List.filter_map
                  (fun (f : Yojson.Basic.t) ->
                    match f with
                    | `Assoc ffields -> (
                        match List.assoc_opt "String" ffields with
                        | Some (`Assoc sfields) -> get_string_field sfields "str"
                        | _ -> None)
                    | _ -> None)
                  fields_list
              in
              (match names with
              | [col] -> Some (None, col)
              | [table; col] -> Some (Some table, col)
              | _ -> None))
          | _ -> None)
      | _ -> None)
  | _ -> None

let resolve_where_params ~default_table tables (params : (int, query_param) Hashtbl.t) (where_json : Yojson.Basic.t) =
  let refs = find_param_refs_in_json where_json in
  List.iter
    (fun (param_num, path) ->
      let column_name =
        match find_parent_named path "A_Expr" with
        | Some expr_fields -> (
            match List.assoc_opt "lexpr" expr_fields with
            | Some lexpr -> (
                match extract_column_ref_from_expr lexpr with
                | Some (_, col) -> col
                | None -> "")
            | None -> "")
        | None -> ""
      in
      let table_name = default_table in
      let ocaml_type, sql_type =
        match List.find_opt (fun (t : table) -> t.name = table_name) tables with
        | Some table -> (
            match Realtime_schema_utils.column_by_name table column_name with
            | Some col -> (Realtime_schema_utils.ocaml_type_of_sql_type col.sql_type, col.sql_type_raw)
            | None -> ("string", "text"))
        | None -> ("string", "text")
      in
      Hashtbl.replace params param_num
        { index = param_num; column_ref = Some (table_name, column_name); ocaml_type; sql_type })
    refs

let resolve_update tables (stmt_fields : (string * Yojson.Basic.t) list) =
  let table_name = Option.value (get_relation_name stmt_fields) ~default:"" in
  let params = Hashtbl.create 4 in
  let targets = get_target_list stmt_fields in
  List.iter
    (fun (target : Yojson.Basic.t) ->
      match target with
      | `Assoc tfields -> (
          match List.assoc_opt "ResTarget" tfields with
          | Some (`Assoc rt_fields) -> (
              let column_name = Option.value (get_string_field rt_fields "name") ~default:"" in
              match List.assoc_opt "val" rt_fields with
              | Some val_json -> (
                  let refs = find_param_refs_in_json val_json in
                  List.iter
                    (fun (param_num, _path) ->
                      let ocaml_type, sql_type =
                        match List.find_opt (fun (t : table) -> t.name = table_name) tables with
                        | Some table -> (
                            match Realtime_schema_utils.column_by_name table column_name with
                            | Some col -> (Realtime_schema_utils.ocaml_type_of_sql_type col.sql_type, col.sql_type_raw)
                            | None -> ("string", "text"))
                        | None -> ("string", "text")
                      in
                      Hashtbl.replace params param_num
                        { index = param_num; column_ref = Some (table_name, column_name); ocaml_type; sql_type })
                    refs)
              | None -> ())
          | _ -> ())
      | _ -> ())
    targets;
  (match get_where_clause stmt_fields with
  | Some wc -> resolve_where_params ~default_table:table_name tables params wc
  | None -> ());
  params

let resolve_delete tables (stmt_fields : (string * Yojson.Basic.t) list) =
  let table_name = Option.value (get_relation_name stmt_fields) ~default:"" in
  let params = Hashtbl.create 4 in
  (match get_where_clause stmt_fields with
  | Some wc -> resolve_where_params ~default_table:table_name tables params wc
  | None -> ());
  params

let build_alias_map (from_clause : Yojson.Basic.t option) =
  let map = Hashtbl.create 4 in
  let rec process (json : Yojson.Basic.t) =
    match json with
    | `Assoc ffields ->
        (match List.assoc_opt "RangeVar" ffields with
        | Some (`Assoc rv_fields) ->
            let relname = get_string_field rv_fields "relname" in
            let alias = get_string_field rv_fields "alias" in
            (match relname, alias with
            | Some r, Some a -> Hashtbl.replace map a r
            | _ -> ())
        | _ ->
            (match List.assoc_opt "JoinExpr" ffields with
            | Some (`Assoc je_fields) ->
                (match List.assoc_opt "larg" je_fields with
                | Some larg -> process larg
                | None -> ());
                (match List.assoc_opt "rarg" je_fields with
                | Some rarg -> process rarg
                | None -> ())
            | _ -> ()))
    | _ -> ()
  in
  (match from_clause with
  | Some (`List items) -> List.iter process items
  | Some (`Assoc _ as json) -> process json
  | _ -> ());
  map

let resolve_column_ref_with_alias alias_map (expr : Yojson.Basic.t) =
  match expr with
  | `Assoc fields -> (
      match List.assoc_opt "ColumnRef" fields with
      | Some (`Assoc cr_fields) -> (
          match List.assoc_opt "fields" cr_fields with
          | Some (`List fields_list) -> (
              let names =
                List.filter_map
                  (fun (f : Yojson.Basic.t) ->
                    match f with
                    | `Assoc ffields -> (
                        match List.assoc_opt "String" ffields with
                        | Some (`Assoc sfields) -> get_string_field sfields "str"
                        | _ -> None)
                    | _ -> None)
                  fields_list
              in
              match names with
              | [col] -> Some (None, col)
              | [alias; col] -> (
                  match Hashtbl.find_opt alias_map alias with
                  | Some table_name -> Some (Some table_name, col)
                  | None -> Some (Some alias, col))
              | _ -> None)
          | _ -> None)
      | _ -> None)
  | _ -> None

let resolve_select tables (stmt_fields : (string * Yojson.Basic.t) list) =
  let alias_map =
    match List.assoc_opt "fromClause" stmt_fields with
    | Some fc -> build_alias_map (Some fc)
    | None -> Hashtbl.create 1
  in
  let from_tables =
    match List.assoc_opt "fromClause" stmt_fields with
    | Some (`List items) ->
        List.filter_map
          (fun (item : Yojson.Basic.t) ->
            match item with
            | `Assoc ffields -> (
                match List.assoc_opt "RangeVar" ffields with
                | Some (`Assoc rv_fields) -> get_string_field rv_fields "relname"
                | _ -> None)
            | _ -> None)
          items
    | _ -> []
  in
  let default_table = match from_tables with [t] -> t | _ -> "" in
  let params = Hashtbl.create 4 in

  let resolve_in_json (json : Yojson.Basic.t) =
    let refs = find_param_refs_in_json json in
    List.iter
      (fun (param_num, path) ->
        let column_name, table_name =
          match find_parent_named path "A_Expr" with
          | Some expr_fields -> (
              match List.assoc_opt "lexpr" expr_fields with
              | Some lexpr -> (
                  match resolve_column_ref_with_alias alias_map lexpr with
                  | Some (Some table, col) -> (col, table)
                  | Some (None, col) -> (col, default_table)
                  | None -> ("", default_table))
              | None -> ("", default_table))
          | None -> ("", default_table)
        in
        let actual_table = if table_name = "" then default_table else table_name in
        let ocaml_type, sql_type =
          match List.find_opt (fun (t : table) -> t.name = actual_table) tables with
          | Some table -> (
              match Realtime_schema_utils.column_by_name table column_name with
              | Some col -> (Realtime_schema_utils.ocaml_type_of_sql_type col.sql_type, col.sql_type_raw)
              | None -> ("string", "text"))
          | None -> ("string", "text")
        in
        Hashtbl.replace params param_num
          { index = param_num; column_ref = Some (actual_table, column_name); ocaml_type; sql_type })
      refs
  in

  (match get_where_clause stmt_fields with
  | Some wc -> resolve_in_json wc
  | None -> ());

  (match List.assoc_opt "fromClause" stmt_fields with
  | Some (`List items) ->
      List.iter
        (fun (item : Yojson.Basic.t) ->
          match item with
          | `Assoc ffields -> (
              match List.assoc_opt "JoinExpr" ffields with
              | Some (`Assoc je_fields) -> (
                  match List.assoc_opt "quals" je_fields with
                  | Some q -> resolve_in_json q
                  | None -> ())
              | _ -> ())
          | _ -> ())
        items
  | _ -> ());

  params

let extract_select_column_name (target : Yojson.Basic.t) =
  let from_column_ref json =
    match extract_column_ref_from_expr json with
    | Some (_, col) -> Some col
    | None -> None
  in
  match target with
  | `Assoc outer_fields -> (
      match List.assoc_opt "ResTarget" outer_fields with
      | Some (`Assoc rt_fields) -> (
          match get_string_field rt_fields "name" with
          | Some alias when alias <> "" -> Some alias
          | _ -> (
              match List.assoc_opt "val" rt_fields with
              | Some val_json -> from_column_ref val_json
              | None -> from_column_ref target))
      | _ -> from_column_ref target)
  | _ -> from_column_ref target

let extract_from_tables (sel_fields : (string * Yojson.Basic.t) list) =
  match List.assoc_opt "fromClause" sel_fields with
  | Some (`List items) ->
      List.filter_map
        (fun (item : Yojson.Basic.t) ->
          match item with
          | `Assoc ffields -> (
              match List.assoc_opt "RangeVar" ffields with
              | Some (`Assoc rv_fields) -> get_string_field rv_fields "relname"
              | _ -> None)
          | _ -> None)
        items
  | _ -> []

let infer_return_table sql =
  match Pg_query.parse sql with
  | Ok tree_json -> (
      let tree : Yojson.Basic.t = Yojson.Basic.from_string tree_json in
      match tree with
      | `Assoc root_fields -> (
          match (List.assoc_opt "stmts" root_fields : Yojson.Basic.t option) with
          | Some (`List (stmts : Yojson.Basic.t list)) -> (
              match List.nth_opt stmts 0 with
              | Some (`Assoc (stmt_wrapper : (string * Yojson.Basic.t) list)) -> (
                  match (List.assoc_opt "stmt" stmt_wrapper : Yojson.Basic.t option) with
                  | Some (`Assoc (stmt_fields : (string * Yojson.Basic.t) list)) -> (
                      match List.assoc_opt "SelectStmt" stmt_fields with
                      | Some (`Assoc (sel_fields : (string * Yojson.Basic.t) list)) -> (
                          match extract_from_tables sel_fields with
                          | [table_name] -> Some table_name
                          | _ -> None)
                      | _ -> None)
                  | _ -> None)
              | _ -> None)
          | _ -> None)
      | _ -> None)
  | Error _ -> None

let infer_select_columns tables (return_table : string option) (sql : string) =
  match Pg_query.parse sql with
  | Ok tree_json -> (
      let tree : Yojson.Basic.t = Yojson.Basic.from_string tree_json in
      match tree with
      | `Assoc root_fields -> (
          match (List.assoc_opt "stmts" root_fields : Yojson.Basic.t option) with
          | Some (`List (stmts : Yojson.Basic.t list)) -> (
              match List.nth_opt stmts 0 with
              | Some (`Assoc (stmt_wrapper : (string * Yojson.Basic.t) list)) -> (
                  match (List.assoc_opt "stmt" stmt_wrapper : Yojson.Basic.t option) with
                  | Some (`Assoc (stmt_fields : (string * Yojson.Basic.t) list)) -> (
                      match List.assoc_opt "SelectStmt" stmt_fields with
                      | Some (`Assoc (sel_fields : (string * Yojson.Basic.t) list)) -> (
                          let target_list =
                            match List.assoc_opt "targetList" sel_fields with
                            | Some (`List (items : Yojson.Basic.t list)) -> items
                            | _ -> []
                          in
                          let table_opt =
                            match return_table with
                            | Some "" -> None
                            | Some t -> List.find_opt (fun (tbl : table) -> tbl.name = t) tables
                            | None -> None
                          in
                          let columns =
                            List.mapi
                              (fun idx target ->
                                let name =
                                  match extract_select_column_name target with
                                  | Some n -> n
                                  | None -> Printf.sprintf "col_%d" (idx + 1)
                                in
                                let sql_type =
                                  match table_opt with
                                  | Some table -> (
                                      match Realtime_schema_utils.column_by_name table name with
                                      | Some col -> col.sql_type
                                      | None -> Text)
                                  | None -> Text
                                in
                                (name, sql_type))
                              target_list
                          in
                          Some columns)
                      | _ -> None)
                  | _ -> None)
              | _ -> None)
          | _ -> None)
      | _ -> None)
  | Error _ -> None

let infer_params tables sql =
  match Pg_query.parse sql with
  | Ok tree_json -> (
      let tree : Yojson.Basic.t = Yojson.Basic.from_string tree_json in
      match tree with
      | `Assoc root_fields -> (
          match (List.assoc_opt "stmts" root_fields : Yojson.Basic.t option) with
          | Some (`List (stmts : Yojson.Basic.t list)) -> (
              match List.nth_opt stmts 0 with
              | Some (`Assoc (stmt_wrapper : (string * Yojson.Basic.t) list)) -> (
                  match (List.assoc_opt "stmt" stmt_wrapper : Yojson.Basic.t option) with
                  | Some (`Assoc (stmt_fields : (string * Yojson.Basic.t) list)) -> (
                      let params =
                        if List.mem_assoc "InsertStmt" stmt_fields then
                          match List.assoc_opt "InsertStmt" stmt_fields with
                          | Some (`Assoc (is_fields : (string * Yojson.Basic.t) list)) ->
                              resolve_insert tables is_fields
                          | _ -> Hashtbl.create 1
                        else if List.mem_assoc "UpdateStmt" stmt_fields then
                          match List.assoc_opt "UpdateStmt" stmt_fields with
                          | Some (`Assoc (us_fields : (string * Yojson.Basic.t) list)) ->
                              resolve_update tables us_fields
                          | _ -> Hashtbl.create 1
                        else if List.mem_assoc "DeleteStmt" stmt_fields then
                          match List.assoc_opt "DeleteStmt" stmt_fields with
                          | Some (`Assoc (ds_fields : (string * Yojson.Basic.t) list)) ->
                              resolve_delete tables ds_fields
                          | _ -> Hashtbl.create 1
                        else if List.mem_assoc "SelectStmt" stmt_fields then
                          match List.assoc_opt "SelectStmt" stmt_fields with
                          | Some (`Assoc (ss_fields : (string * Yojson.Basic.t) list)) ->
                              resolve_select tables ss_fields
                          | _ -> Hashtbl.create 1
                        else
                          Hashtbl.create 1
                      in
                      Hashtbl.to_seq_values params
                      |> List.of_seq
                      |> List.sort (fun a b -> compare a.index b.index))
                  | _ -> [])
              | _ -> [])
          | _ -> [])
      | _ -> [])
  | Error _ -> []
