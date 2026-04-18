open Realtime_schema_types
open Realtime_schema_utils
open Ppxlib
open Ast_builder.Default

let indent block =
  block
  |> String.split_on_char '\n'
  |> List.map (fun line -> if trim line = "" then "" else "  " ^ line)
  |> String.concat "\n"

(* AST-building helper functions using Ast_builder *)
let estring_of_string ~loc s = pexp_constant ~loc (Pconst_string (s, loc, None))

let eint_of_int ~loc n = pexp_constant ~loc (Pconst_integer (string_of_int n, None))

let eident_of_string ~loc s = pexp_ident ~loc (Located.mk ~loc (Lident s))

let lid_of_strings ~loc = function
  | [s] -> Located.mk ~loc (Lident s)
  | xs ->
      let rec make = function
        | [] -> assert false
        | [x] -> Lident x
        | x :: ys -> Ldot (make ys, x)
      in
      Located.mk ~loc (make (List.rev xs))

let epath_of_strings ~loc parts = pexp_ident ~loc (lid_of_strings ~loc parts)

let mpath_of_strings ~loc parts = pmod_ident ~loc (lid_of_strings ~loc parts)

let econstr_ast ~loc name arg = pexp_construct ~loc (Located.mk ~loc (Lident name)) arg

let enone_ast ~loc = econstr_ast ~loc "None" None

let esome_ast ~loc expr = econstr_ast ~loc "Some" (Some expr)

let ebool_ast ~loc value = econstr_ast ~loc (string_of_bool value) None

let platform_attribute ~loc platform_name =
  attribute ~loc ~name:(Located.mk ~loc "platform")
    ~payload:(PStr [pstr_eval ~loc (eident_of_string ~loc platform_name) []])

let native_value_binding ~loc name expr =
  let vb = value_binding ~loc ~pat:(pvar ~loc name) ~expr in
  let vb = {vb with pvb_attributes = [platform_attribute ~loc "native"]} in
  pstr_value ~loc Nonrecursive [vb]

let db_connection_pattern ~loc =
  ppat_constraint ~loc (ppat_unpack ~loc (Located.mk ~loc (Some "Db")))
    (ptyp_package ~loc (Located.mk ~loc (Ldot (Lident "Caqti_lwt", "CONNECTION")), []))

let string_concat_ast ~loc left right =
  pexp_apply ~loc (eident_of_string ~loc "^") [(Nolabel, left); (Nolabel, right)]

let concat_strings_ast ~loc exprs =
  match exprs with
  | [] -> estring_of_string ~loc ""
  | first :: rest -> List.fold_left (string_concat_ast ~loc) first rest

let concat_with_separator_ast ~loc separator exprs =
  match exprs with
  | [] -> estring_of_string ~loc ""
  | first :: rest ->
      List.fold_left
        (fun acc expr -> concat_strings_ast ~loc [acc; estring_of_string ~loc separator; expr])
        first rest

let json_assoc_expr ~loc fields_expr = pexp_variant ~loc "Assoc" (Some fields_expr)

let caqti_type_expr_for_columns_ast ~loc ~type_name (columns : (string * sql_type) list) =
  match columns with
  | [] -> epath_of_strings ~loc ["Caqti_type"; "unit"]
  | _ ->
      let field_names = List.map (fun (name, _) -> sanitize_identifier name) columns in
      let record_body =
        pexp_record ~loc
          (List.map
             (fun name -> (Located.mk ~loc (Lident name), eident_of_string ~loc name))
             field_names)
          None
      in
      let product_fn =
        List.fold_right
          (fun name body -> pexp_fun ~loc Nolabel None (pvar ~loc name) body)
          field_names record_body
      in
      let product_expr =
        pexp_apply ~loc (epath_of_strings ~loc ["Caqti_type"; "product"])
          [(Nolabel, product_fn)]
      in
      let proj_exprs =
        columns
        |> List.map (fun (name, sql_type) ->
               let caqti_ctor =
                 match sql_type with
                 | Uuid | Varchar | Text | Json | Jsonb | Custom _ ->
                     epath_of_strings ~loc ["Caqti_type"; "string"]
                 | Int -> epath_of_strings ~loc ["Caqti_type"; "int"]
                 | Bigint -> epath_of_strings ~loc ["Caqti_type"; "int64"]
                 | Boolean -> epath_of_strings ~loc ["Caqti_type"; "bool"]
                 | Timestamp | Timestamptz -> epath_of_strings ~loc ["Caqti_type"; "float"]
               in
               pexp_apply ~loc (epath_of_strings ~loc ["Caqti_type"; "proj"])
                 [ (Nolabel, caqti_ctor);
                   ( Nolabel,
                     pexp_fun ~loc Nolabel None
                       (ppat_constraint ~loc (pvar ~loc "r")
                          (ptyp_constr ~loc (Located.mk ~loc (Lident type_name)) []))
                       (pexp_field ~loc (eident_of_string ~loc "r")
                          (Located.mk ~loc (Lident (sanitize_identifier name)))) ) ])
      in
      let projection_chain =
        List.fold_right
          (fun proj acc -> pexp_apply ~loc (eident_of_string ~loc "@@") [(Nolabel, proj); (Nolabel, acc)])
          proj_exprs (epath_of_strings ~loc ["Caqti_type"; "proj_end"])
      in
      pexp_apply ~loc (eident_of_string ~loc "@@")
        [(Nolabel, product_expr); (Nolabel, projection_chain)]

let elident_of_strings ~loc = function
  | [s] -> Lident s
  | xs ->
      let rec make = function
        | [] -> assert false
        | [x] -> Lident x
        | x :: ys -> Ldot (make ys, x)
      in
      make (List.rev xs)

let elident_of_mods ~loc mods name =
  List.fold_right (fun a b -> Ldot (b, a)) mods (Lident name)

let eunit_ast ~loc = pexp_construct ~loc (Located.mk ~loc (Lident "()")) None

let eapply_ast ~loc func args = pexp_apply ~loc func args

let elist_ast ~loc exprs = pexp_array ~loc exprs

let elist_of_exprs ~loc exprs =
  let rec make_list = function
    | [] -> pexp_construct ~loc (Located.mk ~loc (Lident "[]")) None
    | x :: xs ->
        pexp_construct ~loc (Located.mk ~loc (Lident "::"))
          (Some (pexp_tuple ~loc [x; make_list xs]))
  in
  make_list exprs

let rlab ~loc name = Located.mk ~loc (Lident name)

let sql_string_literal value = Printf.sprintf "'%s'" (quote_sql_literal value)

let caqti_type_of_ocaml_type_ast ~loc = function
  | "string" -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "string")))
  | "int" -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "int")))
  | "int64" -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "int64")))
  | "bool" -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "bool")))
  | "float" -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "float")))
  | _ -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "string")))

let caqti_type_constructor_of_sql_type_ast ~loc = function
  | Uuid | Varchar | Text | Json | Jsonb | Custom _ -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "string")))
  | Int -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "int")))
  | Bigint -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "int64")))
  | Boolean -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "bool")))
  | Timestamp | Timestamptz -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "float")))

let mutation_param_type_expr_ast ~loc params =
  let type_of_param p = caqti_type_of_ocaml_type_ast ~loc p.ocaml_type in
  match params with
  | [] -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "unit")))
  | [ p ] -> type_of_param p
  | [ p1; p2 ] -> pexp_apply ~loc (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "t2")))) [(Nolabel, type_of_param p1); (Nolabel, type_of_param p2)]
  | [ p1; p2; p3 ] -> pexp_apply ~loc (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "t3")))) [(Nolabel, type_of_param p1); (Nolabel, type_of_param p2); (Nolabel, type_of_param p3)]
  | [ p1; p2; p3; p4 ] -> pexp_apply ~loc (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "t4")))) [(Nolabel, type_of_param p1); (Nolabel, type_of_param p2); (Nolabel, type_of_param p3); (Nolabel, type_of_param p4)]
  | [ p1; p2; p3; p4; p5 ] -> pexp_apply ~loc (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "t5")))) [(Nolabel, type_of_param p1); (Nolabel, type_of_param p2); (Nolabel, type_of_param p3); (Nolabel, type_of_param p4); (Nolabel, type_of_param p5)]
  | _ -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "unit")))

let params_literal_ast ~loc params =
  let param_to_record_expr param =
    let fields =
      [
        (rlab ~loc "index", pexp_constant ~loc (Pconst_integer (string_of_int param.index, None)));
        (rlab ~loc "column_ref",
          (match param.column_ref with
           | None -> enone_ast ~loc
           | Some (table_name, column_name) ->
             esome_ast ~loc
               (pexp_tuple ~loc
                  [ pexp_constant ~loc (Pconst_string (table_name, loc, None));
                    pexp_constant ~loc (Pconst_string (column_name, loc, None)) ])));
        (rlab ~loc "payload_key",
          (match param.payload_key with
           | None -> enone_ast ~loc
           | Some key -> esome_ast ~loc (pexp_constant ~loc (Pconst_string (key, loc, None)))));
        (rlab ~loc "ocaml_type", pexp_constant ~loc (Pconst_string (param.ocaml_type, loc, None)));
        (rlab ~loc "sql_type", pexp_constant ~loc (Pconst_string (param.sql_type, loc, None)));
      ]
    in
    pexp_record ~loc fields None
  in
  elist_of_exprs ~loc (List.map param_to_record_expr params)

let ocaml_type_string_of_sql_type sql_type =
  match ocaml_type_of_sql_type sql_type with
  | "string" -> "string"
  | "int" -> "int"
  | "int64" -> "int64"
  | "bool" -> "bool"
  | "float" -> "float"
  | "Yojson.Safe.t" -> "Yojson.Safe.t"
  | other -> other

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

let encode_params_code_ast ~loc (params : query_param list) =
  match params with
  | [] ->
      pstr_value ~loc Nonrecursive
        [ value_binding ~loc ~pat:(pvar ~loc "encodeParams")
            ~expr:(pexp_fun ~loc Nolabel None (ppat_construct ~loc (Located.mk ~loc (Lident "()")) None)
                     (epath_of_strings ~loc ["Melange_json"; "Primitives"; "unit_to_json"] |> fun fn ->
                      pexp_apply ~loc fn [(Nolabel, eunit_ast ~loc)])) ]
  | _ ->
      let dict_binding =
        value_binding ~loc ~pat:(pvar ~loc "dict")
          ~expr:(pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "empty"]) [(Nolabel, eunit_ast ~loc)])
      in
      let set_calls =
        params
        |> List.map (fun (param : query_param) ->
               let field_name =
                 match param.payload_key with
                 | Some key -> sanitize_identifier key
                 | None -> Printf.sprintf "param_%d" param.index
               in
               let json_fn = epath_of_strings ~loc ["Melange_json"; "Primitives"; ocaml_type_to_json_fn param.ocaml_type] in
               pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "set"])
                 [ (Nolabel, eident_of_string ~loc "dict");
                   (Nolabel, estring_of_string ~loc field_name);
                   ( Nolabel,
                     pexp_apply ~loc json_fn
                       [ (Nolabel, pexp_field ~loc (eident_of_string ~loc "p") (Located.mk ~loc (Lident field_name))) ] ) ])
      in
      let body =
        List.fold_right
          (fun call acc -> pexp_sequence ~loc call acc)
          set_calls
          (pexp_apply ~loc (epath_of_strings ~loc ["Melange_json"; "declassify"])
             [ ( Nolabel,
                 json_assoc_expr ~loc
                   (pexp_apply ~loc (epath_of_strings ~loc ["Array"; "to_list"])
                      [ ( Nolabel,
                          pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "entries"])
                            [(Nolabel, eident_of_string ~loc "dict")] ) ]) ) ])
      in
      pstr_value ~loc Nonrecursive
        [ value_binding ~loc ~pat:(pvar ~loc "encodeParams")
            ~expr:(pexp_fun ~loc Nolabel None
                     (ppat_constraint ~loc (pvar ~loc "p")
                        (ptyp_constr ~loc (Located.mk ~loc (Lident "params")) []))
                     (pexp_let ~loc Nonrecursive [dict_binding] body)) ]

(* AST-building versions of record literals *)

let ocaml_type_of_sql_type_ast ~loc (sql_type : sql_type) : expression =
  match sql_type with
  | Uuid -> econstr_ast ~loc "Uuid" None
  | Varchar -> econstr_ast ~loc "Varchar" None
  | Text -> econstr_ast ~loc "Text" None
  | Int -> econstr_ast ~loc "Int" None
  | Bigint -> econstr_ast ~loc "Bigint" None
  | Boolean -> econstr_ast ~loc "Boolean" None
  | Timestamp -> econstr_ast ~loc "Timestamp" None
  | Timestamptz -> econstr_ast ~loc "Timestamptz" None
  | Json -> econstr_ast ~loc "Json" None
  | Jsonb -> econstr_ast ~loc "Jsonb" None
  | Custom value -> econstr_ast ~loc "Custom" (Some (estring_of_string ~loc value))

let foreign_key_to_ast ~loc (fk : foreign_key option) : expression =
  match fk with
  | None -> enone_ast ~loc
  | Some fk ->
      esome_ast ~loc
        (pexp_record ~loc [
             Located.mk ~loc (Lident "column"), estring_of_string ~loc fk.column;
             Located.mk ~loc (Lident "referenced_table"), estring_of_string ~loc fk.referenced_table;
             Located.mk ~loc (Lident "referenced_column"), estring_of_string ~loc fk.referenced_column;
          ] None)

let broadcast_channel_to_ast ~loc (bc : broadcast_channel option) : expression =
  match bc with
  | None -> enone_ast ~loc
  | Some (Column value) -> esome_ast ~loc (econstr_ast ~loc "Column" (Some (estring_of_string ~loc value)))
  | Some (Computed value) -> esome_ast ~loc (econstr_ast ~loc "Computed" (Some (estring_of_string ~loc value)))
  | Some (Conditional value) -> esome_ast ~loc (econstr_ast ~loc "Conditional" (Some (estring_of_string ~loc value)))
  | Some (Subquery value) -> esome_ast ~loc (econstr_ast ~loc "Subquery" (Some (estring_of_string ~loc value)))

let broadcast_parent_to_ast ~loc (bp : broadcast_parent option) : expression =
  match bp with
  | None -> enone_ast ~loc
  | Some bp ->
      esome_ast ~loc
        (pexp_record ~loc [
             Located.mk ~loc (Lident "parent_table"), estring_of_string ~loc bp.parent_table;
             Located.mk ~loc (Lident "query_name"), estring_of_string ~loc bp.query_name;
          ] None)

let broadcast_to_views_to_ast ~loc (btv : broadcast_to_views option) : expression =
  match btv with
  | None -> enone_ast ~loc
  | Some config ->
      esome_ast ~loc
        (pexp_record ~loc [
             Located.mk ~loc (Lident "view_table"), estring_of_string ~loc config.view_table;
             Located.mk ~loc (Lident "channel_column"), estring_of_string ~loc config.channel_column;
          ] None)

let column_record_literal_ast ~loc (column : column) : expression =
  pexp_record ~loc [
    Located.mk ~loc (Lident "name"), estring_of_string ~loc column.name;
    Located.mk ~loc (Lident "sql_type"), ocaml_type_of_sql_type_ast ~loc column.sql_type;
    Located.mk ~loc (Lident "sql_type_raw"), estring_of_string ~loc column.sql_type_raw;
    Located.mk ~loc (Lident "nullable"), ebool_ast ~loc column.nullable;
    Located.mk ~loc (Lident "primary_key"), ebool_ast ~loc column.primary_key;
    Located.mk ~loc (Lident "default"), 
      (match column.default with
       | None -> enone_ast ~loc
       | Some v -> esome_ast ~loc (estring_of_string ~loc v));
    Located.mk ~loc (Lident "foreign_key"), foreign_key_to_ast ~loc column.foreign_key;
    Located.mk ~loc (Lident "definition_sql"), estring_of_string ~loc column.definition_sql;
  ] None

let table_record_literal_ast ~loc (table : table) : expression =
  let columns_expr = elist_of_exprs ~loc (List.map (column_record_literal_ast ~loc) table.columns) in
  pexp_record ~loc [
    Located.mk ~loc (Lident "name"), estring_of_string ~loc table.name;
    Located.mk ~loc (Lident "columns"), columns_expr;
    Located.mk ~loc (Lident "id_column"),
      (match table.id_column with
       | None -> enone_ast ~loc
       | Some v -> esome_ast ~loc (estring_of_string ~loc v));
    Located.mk ~loc (Lident "composite_key"), elist_of_exprs ~loc (List.map (estring_of_string ~loc) table.composite_key);
    Located.mk ~loc (Lident "broadcast_channel"), broadcast_channel_to_ast ~loc table.broadcast_channel;
    Located.mk ~loc (Lident "broadcast_parent"), broadcast_parent_to_ast ~loc table.broadcast_parent;
    Located.mk ~loc (Lident "broadcast_to_views"), broadcast_to_views_to_ast ~loc table.broadcast_to_views;
    Located.mk ~loc (Lident "create_sql"), estring_of_string ~loc table.create_sql;
    Located.mk ~loc (Lident "source_file"), estring_of_string ~loc table.source_file;
  ] None

let string_of_handler_ast ~loc = function
  | Sql -> econstr_ast ~loc "Sql" None
  | Ocaml -> econstr_ast ~loc "Ocaml" None

let query_record_literal_ast ~loc (query : query) : expression =
  pexp_record ~loc [
    Located.mk ~loc (Lident "name"), estring_of_string ~loc query.name;
    Located.mk ~loc (Lident "sql"), estring_of_string ~loc query.sql;
    Located.mk ~loc (Lident "source_file"), estring_of_string ~loc query.source_file;
     Located.mk ~loc (Lident "cache_key"),
       (match query.cache_key with
       | None -> enone_ast ~loc
       | Some v -> esome_ast ~loc (estring_of_string ~loc v));
     Located.mk ~loc (Lident "return_table"),
       (match query.return_table with
       | None -> enone_ast ~loc
       | Some v -> esome_ast ~loc (estring_of_string ~loc v));
    Located.mk ~loc (Lident "json_columns"), elist_of_exprs ~loc (List.map (estring_of_string ~loc) query.json_columns);
    Located.mk ~loc (Lident "params_metadata"), params_literal_ast ~loc query.params;
    Located.mk ~loc (Lident "handler"), string_of_handler_ast ~loc query.handler;
  ] None

let mutation_record_literal_ast ~loc (mutation : mutation) : expression =
  pexp_record ~loc [
    Located.mk ~loc (Lident "name"), estring_of_string ~loc mutation.name;
    Located.mk ~loc (Lident "sql"), estring_of_string ~loc mutation.sql;
    Located.mk ~loc (Lident "source_file"), estring_of_string ~loc mutation.source_file;
    Located.mk ~loc (Lident "params_metadata"), params_literal_ast ~loc mutation.params;
    Located.mk ~loc (Lident "handler"), string_of_handler_ast ~loc mutation.handler;
  ] None

(* AST-building versions of type declarations *)

let column_type_annotation_ast ~loc (column : column) : core_type =
  let type_str = ocaml_type_string_of_sql_type column.sql_type in
  ptyp_constr ~loc (Located.mk ~loc (Lident type_str)) []

let record_type_declaration_ast ~loc (table : table) : structure_item =
  let type_name = type_name_of_table table.name in
  let label_decls =
    table.columns
    |> List.map (fun (column : column) ->
         label_declaration
           ~loc
           ~name:(Located.mk ~loc (sanitize_identifier column.name))
           ~mutable_:Immutable
           ~type_:(column_type_annotation_ast ~loc column))
  in
  pstr_type ~loc Nonrecursive [
    type_declaration
      ~loc
      ~name:(Located.mk ~loc type_name)
      ~params:[]
      ~cstrs:[]
      ~private_:Public
      ~manifest:None
      ~kind:(Ptype_record label_decls)
  ]

let params_type_declaration_ast ~loc (params : query_param list) : structure_item =
  match params with
  | [] ->
      pstr_type ~loc Nonrecursive [
        type_declaration ~loc ~name:(Located.mk ~loc "params") ~params:[] ~cstrs:[]
          ~private_:Public ~manifest:(Some (ptyp_constr ~loc (Located.mk ~loc (Lident "unit")) []))
          ~kind:Ptype_abstract
      ]
  | _ ->
      let label_decls =
        params
        |> List.map (fun (param : query_param) ->
               let field_name =
                 match param.payload_key with
                 | Some key -> sanitize_identifier key
                 | None -> Printf.sprintf "param_%d" param.index
               in
               label_declaration ~loc ~name:(Located.mk ~loc field_name) ~mutable_:Immutable
                 ~type_:(ptyp_constr ~loc (Located.mk ~loc (Lident param.ocaml_type)) []))
      in
      pstr_type ~loc Nonrecursive [
        type_declaration ~loc ~name:(Located.mk ~loc "params") ~params:[] ~cstrs:[] ~private_:Public
          ~manifest:None ~kind:(Ptype_record label_decls)
      ]

let caqti_type_constructor_sql_type_ast ~loc = function
  | Uuid | Varchar | Text | Json | Jsonb | Custom _ -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "string")))
  | Int -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "int")))
  | Bigint -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "int64")))
  | Boolean -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "bool")))
  | Timestamp | Timestamptz -> pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Caqti_type", "float")))

let table_caqti_type_declaration_ast ~loc (table : table) : structure_item =
  let columns = List.map (fun (c : column) -> (c.name, c.sql_type)) table.columns in
  pstr_eval ~loc (caqti_type_expr_for_columns_ast ~loc ~type_name:(type_name_of_table table.name) columns) []

let table_module_declaration_ast ~loc (table : table) : structure_item list =
  let module_name = module_name_of_table table.name in
  (* let name = "table_name" *)
  let name_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "name")
          ~expr:(estring_of_string ~loc table.name) ]
  in
  (* let id_column = Some "col" | None *)
  let id_column_expr =
    match table.id_column with
    | None -> pexp_construct ~loc (Located.mk ~loc (Lident "None")) None
    | Some value ->
        pexp_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (estring_of_string ~loc value))
  in
  let id_column_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "id_column")
          ~expr:id_column_expr ]
  in
  (* let composite_key = ["col1"; "col2"] *)
  let composite_key_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "composite_key")
          ~expr:(elist_of_exprs ~loc (List.map (estring_of_string ~loc) table.composite_key)) ]
  in
  (* let caqti_type = ... *)
  let caqti_type_structure_item = table_caqti_type_declaration_ast ~loc table in
  let caqti_type_expr =
    match caqti_type_structure_item.pstr_desc with
    | Pstr_eval (expr, _) -> expr
    | _ -> assert false
  in
  let caqti_type_binding =
    let vb = value_binding ~loc ~pat:(pvar ~loc "caqti_type") ~expr:caqti_type_expr in
    let vb = {vb with pvb_attributes = [platform_attribute ~loc "native"]} in
    pstr_value ~loc Nonrecursive [vb]
  in
  let structure_items = [ name_binding; id_column_binding; composite_key_binding; caqti_type_binding ] in
  let module_expr = pmod_structure ~loc structure_items in
  let mod_binding =
    { pmb_name = Located.mk ~loc (Some module_name);
      pmb_expr = module_expr;
      pmb_attributes = [];
      pmb_loc = loc }
  in
  [ pstr_module ~loc mod_binding ]

let params_hash_code_ast ~loc (params : query_param list) =
  let string_expr_for_param param =
    let field_name =
      match param.payload_key with
      | Some key -> sanitize_identifier key
      | None -> Printf.sprintf "param_%d" param.index
    in
    let field_expr = pexp_field ~loc (eident_of_string ~loc "p") (Located.mk ~loc (Lident field_name)) in
    match string_conversion_fn param.ocaml_type with
    | "" -> field_expr
    | fn -> pexp_apply ~loc (eident_of_string ~loc fn) [(Nolabel, field_expr)]
  in
  match params with
  | [] ->
      pstr_value ~loc Nonrecursive
        [ value_binding ~loc ~pat:(pvar ~loc "paramsHash")
            ~expr:(pexp_fun ~loc Nolabel None (ppat_construct ~loc (Located.mk ~loc (Lident "()")) None)
                     (estring_of_string ~loc "")) ]
  | _ ->
      let hash_body = concat_with_separator_ast ~loc ":" (List.map string_expr_for_param params) in
      pstr_value ~loc Nonrecursive
        [ value_binding ~loc ~pat:(pvar ~loc "paramsHash")
            ~expr:(pexp_fun ~loc Nolabel None
                     (ppat_constraint ~loc (pvar ~loc "p")
                        (ptyp_constr ~loc (Located.mk ~loc (Lident "params")) []))
                     hash_body) ]

let caqti_param_expr_ast ~loc (params : query_param list) =
  match params with
  | [] -> pexp_construct ~loc (Located.mk ~loc (Lident "()")) None
  | [ param ] ->
      let field_name =
        match param.payload_key with
        | Some key -> sanitize_identifier key
        | None -> Printf.sprintf "param_%d" param.index
      in
      pexp_field ~loc (pexp_ident ~loc (Located.mk ~loc (Lident "p"))) (Located.mk ~loc (Lident field_name))
  | _ ->
      let fields =
        params
        |> List.map (fun (param : query_param) ->
              let field_name =
                match param.payload_key with
                | Some key -> sanitize_identifier key
                | None -> Printf.sprintf "param_%d" param.index
              in
              pexp_field ~loc (pexp_ident ~loc (Located.mk ~loc (Lident "p"))) (Located.mk ~loc (Lident field_name)))
      in
      pexp_tuple ~loc fields

let channel_function_code_ast ~loc tables (query : query) =
  pstr_value ~loc Nonrecursive
    [ value_binding ~loc ~pat:(pvar ~loc "channel")
        ~expr:(pexp_fun ~loc Nolabel None
                 (pvar ~loc "_")
                 (estring_of_string ~loc query.name)) ]

let decode_row_code_ast ~loc (columns : (string * sql_type) list) =
  match columns with
  | [] ->
      pstr_value ~loc Nonrecursive
        [ value_binding ~loc ~pat:(pvar ~loc "decodeRow")
            ~expr:(pexp_fun ~loc Nolabel None (pvar ~loc "_") (eunit_ast ~loc)) ]
  | _ ->
      let fields =
        columns
        |> List.map (fun (name, sql_type) ->
               let field_name = sanitize_identifier name in
               let decode_fn = epath_of_strings ~loc ["Melange_json"; "Primitives"; ocaml_type_of_json_fn (ocaml_type_string_of_sql_type sql_type)] in
               ( Located.mk ~loc (Lident field_name),
                 pexp_apply ~loc (epath_of_strings ~loc ["StoreJson"; "requiredField"])
                   [ (Labelled "json", eident_of_string ~loc "json");
                     (Labelled "fieldName", estring_of_string ~loc name);
                     (Labelled "decode", decode_fn) ] ))
      in
      pstr_value ~loc Nonrecursive
        [ value_binding ~loc ~pat:(pvar ~loc "decodeRow")
            ~expr:(pexp_fun ~loc Nolabel None (pvar ~loc "json") (pexp_record ~loc fields None)) ]

let row_to_json_code_ast ~loc (columns : (string * sql_type) list) =
  match columns with
  | [] ->
      pstr_value ~loc Nonrecursive
        [ value_binding ~loc ~pat:(pvar ~loc "row_to_json")
            ~expr:(pexp_fun ~loc Nolabel None (pvar ~loc "_")
                     (pexp_apply ~loc (epath_of_strings ~loc ["Melange_json"; "Primitives"; "unit_to_json"])
                        [(Nolabel, eunit_ast ~loc)])) ]
  | _ ->
      let dict_binding =
        value_binding ~loc ~pat:(pvar ~loc "dict")
          ~expr:(pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "empty"]) [(Nolabel, eunit_ast ~loc)])
      in
      let set_calls =
        columns
        |> List.map (fun (name, sql_type) ->
               let field_name = sanitize_identifier name in
               let json_fn = epath_of_strings ~loc ["Melange_json"; "Primitives"; ocaml_type_to_json_fn (ocaml_type_string_of_sql_type sql_type)] in
               pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "set"])
                 [ (Nolabel, eident_of_string ~loc "dict");
                   (Nolabel, estring_of_string ~loc name);
                   ( Nolabel,
                     pexp_apply ~loc json_fn
                       [ (Nolabel, pexp_field ~loc (eident_of_string ~loc "row") (Located.mk ~loc (Lident field_name))) ] ) ])
      in
      let body =
        List.fold_right
          (fun call acc -> pexp_sequence ~loc call acc)
          set_calls
          (pexp_apply ~loc (epath_of_strings ~loc ["Melange_json"; "declassify"])
             [ ( Nolabel,
                 json_assoc_expr ~loc
                   (pexp_apply ~loc (epath_of_strings ~loc ["Array"; "to_list"])
                      [ ( Nolabel,
                          pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "entries"])
                            [(Nolabel, eident_of_string ~loc "dict")] ) ]) ) ])
      in
      pstr_value ~loc Nonrecursive
        [ value_binding ~loc ~pat:(pvar ~loc "row_to_json")
            ~expr:(pexp_fun ~loc Nolabel None
                     (ppat_constraint ~loc (pvar ~loc "row")
                        (ptyp_constr ~loc (Located.mk ~loc (Lident "row")) []))
                     (pexp_let ~loc Nonrecursive [dict_binding] body)) ]

let query_module_declaration_ast ~loc (tables : table list) (query : query) : structure_item list =
  let mod_name = module_name_of_table query.name in
  let inferred_columns = Realtime_schema_pg_query.infer_select_columns tables query.return_table query.sql in
  let params_metadata_expr =
    elist_of_exprs ~loc
      (List.map
         (fun (param : query_param) ->
           pexp_tuple ~loc
             [eint_of_int ~loc param.index; estring_of_string ~loc param.ocaml_type; estring_of_string ~loc param.sql_type])
         query.params)
  in
  let return_table_expr =
    match query.return_table with
    | None -> enone_ast ~loc
    | Some v -> esome_ast ~loc (estring_of_string ~loc v)
  in
  let json_columns_expr = elist_of_exprs ~loc (List.map (estring_of_string ~loc) query.json_columns) in
  let handler_expr = string_of_handler_ast ~loc query.handler in
  let row_columns = Option.value inferred_columns ~default:[] in
  let row_type_decl =
    if row_columns = [] then
      pstr_type ~loc Nonrecursive
        [ type_declaration ~loc ~name:(Located.mk ~loc "row") ~params:[] ~cstrs:[] ~private_:Public
            ~manifest:(Some (ptyp_constr ~loc (Located.mk ~loc (Lident "unit")) []))
            ~kind:Ptype_abstract ]
    else
      let fields =
        row_columns
        |> List.map (fun (name, sql_type) ->
               label_declaration ~loc ~name:(Located.mk ~loc (sanitize_identifier name)) ~mutable_:Immutable
                 ~type_:(ptyp_constr ~loc (Located.mk ~loc (Lident (ocaml_type_string_of_sql_type sql_type))) []))
      in
      pstr_type ~loc Nonrecursive
        [ type_declaration ~loc ~name:(Located.mk ~loc "row") ~params:[] ~cstrs:[] ~private_:Public
            ~manifest:None ~kind:(Ptype_record fields) ]
  in
  let param_type_expr = mutation_param_type_expr_ast ~loc query.params in
  let caqti_type_expr =
    caqti_type_expr_for_columns_ast ~loc ~type_name:"row"
      (List.map (fun (name, sql_type) -> (name, sql_type)) row_columns)
  in
  let infix_request_expr operator right_expr =
    let request_builder =
      pexp_open ~loc
        (open_infos ~loc ~expr:(mpath_of_strings ~loc ["Caqti_request"; "Infix"]) ~override:Fresh)
        (pexp_apply ~loc (eident_of_string ~loc operator)
           [(Nolabel, eident_of_string ~loc "param_type"); (Nolabel, right_expr)])
    in
    pexp_apply ~loc request_builder [(Nolabel, eident_of_string ~loc "sql")]
  in
  let name_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "name") ~expr:(estring_of_string ~loc query.name) ]
  in
  let sql_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "sql") ~expr:(estring_of_string ~loc query.sql) ]
  in
  let params_metadata_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "params_metadata") ~expr:params_metadata_expr ]
  in
  let return_table_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "return_table") ~expr:return_table_expr ]
  in
  let json_columns_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "json_columns") ~expr:json_columns_expr ]
  in
  let handler_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "handler") ~expr:handler_expr ]
  in
  let caqti_type_binding = native_value_binding ~loc "caqti_type" caqti_type_expr in
  let param_type_binding = native_value_binding ~loc "param_type" param_type_expr in
  let request_binding =
    native_value_binding ~loc "request"
      (pexp_fun ~loc Nolabel None (pvar ~loc "row_type")
         (infix_request_expr "->*" (eident_of_string ~loc "row_type")))
  in
  let find_request_binding =
    native_value_binding ~loc "find_request"
      (pexp_fun ~loc Nolabel None (pvar ~loc "row_type")
         (infix_request_expr "->?" (eident_of_string ~loc "row_type")))
  in
  let collect_binding =
    native_value_binding ~loc "collect"
      (pexp_fun ~loc Nolabel None (db_connection_pattern ~loc)
         (pexp_fun ~loc Nolabel None (pvar ~loc "row_type")
            (pexp_fun ~loc Nolabel None (pvar ~loc "param_values")
               (pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "bind"])
                  [ ( Nolabel,
                      pexp_apply ~loc (epath_of_strings ~loc ["Db"; "collect_list"])
                        [ (Nolabel, pexp_apply ~loc (eident_of_string ~loc "request") [(Nolabel, eident_of_string ~loc "row_type")]);
                          (Nolabel, eident_of_string ~loc "param_values") ] );
                    (Nolabel, epath_of_strings ~loc ["Caqti_lwt"; "or_fail"]) ]))))
  in
  let find_opt_binding =
    native_value_binding ~loc "find_opt"
      (pexp_fun ~loc Nolabel None (db_connection_pattern ~loc)
         (pexp_fun ~loc Nolabel None (pvar ~loc "row_type")
            (pexp_fun ~loc Nolabel None (pvar ~loc "param_values")
               (pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "bind"])
                  [ ( Nolabel,
                      pexp_apply ~loc (epath_of_strings ~loc ["Db"; "find_opt"])
                        [ (Nolabel, pexp_apply ~loc (eident_of_string ~loc "find_request") [(Nolabel, eident_of_string ~loc "row_type")]);
                          (Nolabel, eident_of_string ~loc "param_values") ] );
                    (Nolabel, epath_of_strings ~loc ["Caqti_lwt"; "or_fail"]) ]))))
  in
  let params_type_binding = params_type_declaration_ast ~loc query.params in
  let channel_binding = channel_function_code_ast ~loc tables query in
  let params_hash_binding = params_hash_code_ast ~loc query.params in
  let encode_params_binding = encode_params_code_ast ~loc query.params in
  let decode_row_binding = decode_row_code_ast ~loc row_columns in
  let row_to_json_binding = row_to_json_code_ast ~loc row_columns in
  let execute_binding =
    let caqti_args = caqti_param_expr_ast ~loc query.params in
    let success_body =
      pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "bind"])
        [ ( Nolabel,
            pexp_apply ~loc (eident_of_string ~loc "collect")
              [ (Nolabel, pexp_pack ~loc (mpath_of_strings ~loc ["Db"]));
                (Nolabel, eident_of_string ~loc "caqti_type");
                (Nolabel, caqti_args) ] );
          ( Nolabel,
            pexp_fun ~loc Nolabel None (pvar ~loc "rows")
              (pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "return"])
                 [ ( Nolabel,
                     econstr_ast ~loc "Ok"
                       (Some
                          (pexp_apply ~loc (epath_of_strings ~loc ["Array"; "of_list"])
                             [(Nolabel, eident_of_string ~loc "rows")])) ) ]) ) ]
    in
    let error_handler =
      pexp_function_cases ~loc
        [ case
            ~lhs:(ppat_construct ~loc (Located.mk ~loc (Ldot (Lident "Caqti_error", "Exn")))
                    (Some (pvar ~loc "error")))
            ~guard:None
            ~rhs:(pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "return"])
                    [ ( Nolabel,
                        econstr_ast ~loc "Error"
                          (Some
                             (pexp_apply ~loc (epath_of_strings ~loc ["Caqti_error"; "show"])
                                [(Nolabel, eident_of_string ~loc "error")])) ) ]);
          case ~lhs:(pvar ~loc "exn") ~guard:None
            ~rhs:(pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "return"])
                    [ ( Nolabel,
                        econstr_ast ~loc "Error"
                          (Some
                             (pexp_apply ~loc (epath_of_strings ~loc ["Printexc"; "to_string"])
                                [(Nolabel, eident_of_string ~loc "exn")])) ) ]) ]
    in
    native_value_binding ~loc "execute"
      (pexp_fun ~loc Nolabel None (db_connection_pattern ~loc)
         (pexp_fun ~loc Nolabel None
            (ppat_constraint ~loc (pvar ~loc "p") (ptyp_constr ~loc (Located.mk ~loc (Lident "params")) []))
            (pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "catch"])
               [ (Nolabel, pexp_fun ~loc Nolabel None (ppat_construct ~loc (Located.mk ~loc (Lident "()")) None) success_body);
                 (Nolabel, error_handler) ])))
  in
  let structure_items = [
    name_binding;
    sql_binding;
    params_metadata_binding;
    return_table_binding;
    json_columns_binding;
    handler_binding;
    row_type_decl;
    caqti_type_binding;
    param_type_binding;
    request_binding;
    find_request_binding;
    collect_binding;
    find_opt_binding;
    params_type_binding;
    channel_binding;
    params_hash_binding;
    encode_params_binding;
    decode_row_binding;
    row_to_json_binding;
    execute_binding;
  ] in
  [ pstr_module ~loc
      (module_binding ~loc
         ~name:(Located.mk ~loc (Some mod_name))
         ~expr:(pmod_structure ~loc structure_items))
  ]

let json_extractor_cases_ast ~loc ~ocaml_type =
  let ok e = pexp_construct ~loc (Located.mk ~loc (Lident "Ok")) (Some e) in
  let error_msg = pexp_construct ~loc (Located.mk ~loc (Lident "Error")) (Some (eident_of_string ~loc "msg")) in
  let wild_case = case ~lhs:(ppat_any ~loc) ~guard:None ~rhs:error_msg in
  match ocaml_type with
  | "bool" ->
      let bool_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "Bool" (Some (pvar ~loc "b")))))
          ~guard:None ~rhs:(ok (eident_of_string ~loc "b"))
      in
      let true_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "String"
            (Some (ppat_constant ~loc (Pconst_string ("true", loc, None)))))))
          ~guard:None ~rhs:(ok (pexp_construct ~loc (Located.mk ~loc (Lident "true")) None))
      in
      let false_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "String"
            (Some (ppat_constant ~loc (Pconst_string ("false", loc, None)))))))
          ~guard:None ~rhs:(ok (pexp_construct ~loc (Located.mk ~loc (Lident "false")) None))
      in
      [bool_case; true_case; false_case; wild_case]
  | "int" ->
      let int_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "Int" (Some (pvar ~loc "i")))))
          ~guard:None ~rhs:(ok (eident_of_string ~loc "i"))
      in
      let float_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "Float" (Some (pvar ~loc "f")))))
          ~guard:None ~rhs:(ok (pexp_apply ~loc (eident_of_string ~loc "int_of_float")
            [(Nolabel, eident_of_string ~loc "f")]))
      in
      let str_case =
        let try_body = ok (pexp_apply ~loc (eident_of_string ~loc "int_of_string")
          [(Nolabel, eident_of_string ~loc "s")]) in
        let failure_case =
          case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Failure")) (Some (pvar ~loc "_")))
            ~guard:None ~rhs:error_msg
        in
        let try_expr = pexp_try ~loc try_body [failure_case] in
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "String" (Some (pvar ~loc "s")))))
          ~guard:None ~rhs:try_expr
      in
      [int_case; float_case; str_case; wild_case]
  | "float" ->
      let float_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "Float" (Some (pvar ~loc "f")))))
          ~guard:None ~rhs:(ok (eident_of_string ~loc "f"))
      in
      let int_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "Int" (Some (pvar ~loc "i")))))
          ~guard:None ~rhs:(ok (pexp_apply ~loc (eident_of_string ~loc "float_of_int")
            [(Nolabel, eident_of_string ~loc "i")]))
      in
      let str_case =
        let try_body = ok (pexp_apply ~loc (eident_of_string ~loc "float_of_string")
          [(Nolabel, eident_of_string ~loc "s")]) in
        let failure_case =
          case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Failure")) (Some (pvar ~loc "_")))
            ~guard:None ~rhs:error_msg
        in
        let try_expr = pexp_try ~loc try_body [failure_case] in
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "String" (Some (pvar ~loc "s")))))
          ~guard:None ~rhs:try_expr
      in
      [float_case; int_case; str_case; wild_case]
  | "int64" ->
      let int_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "Int" (Some (pvar ~loc "i")))))
          ~guard:None ~rhs:(ok (pexp_apply ~loc (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Int64", "of_int"))))
            [(Nolabel, eident_of_string ~loc "i")]))
      in
      let str_case =
        let try_body = ok (pexp_apply ~loc (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "Int64", "of_string"))))
          [(Nolabel, eident_of_string ~loc "s")]) in
        let failure_case =
          case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Failure")) (Some (pvar ~loc "_")))
            ~guard:None ~rhs:error_msg
        in
        let try_expr = pexp_try ~loc try_body [failure_case] in
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "String" (Some (pvar ~loc "s")))))
          ~guard:None ~rhs:try_expr
      in
      [int_case; str_case; wild_case]
  | _ ->
      let str_case =
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some"))
          (Some (ppat_variant ~loc "String" (Some (pvar ~loc "s")))))
          ~guard:None ~rhs:(ok (eident_of_string ~loc "s"))
      in
      [str_case; wild_case]

let dispatch_function_for_mutation_ast ~loc (mutation : mutation) : structure_item option =
  match mutation.handler with
  | Ocaml -> None
  | Sql ->
      let params = mutation.params in
      let var_names =
        params
        |> List.map (fun (param : query_param) ->
          let key = Option.value param.payload_key ~default:(Printf.sprintf "param_%d" param.index) in
          sanitize_identifier key)
      in
      let exec_arg =
        match var_names with
        | [] -> eunit_ast ~loc
        | [v] -> eident_of_string ~loc v
        | vs -> pexp_tuple ~loc (List.map (fun v -> eident_of_string ~loc v) vs)
      in
      let exec_call =
        pexp_apply ~loc (eident_of_string ~loc "exec")
          [(Nolabel, pexp_pack ~loc (pmod_ident ~loc (Located.mk ~loc (Lident "Db"))));
            (Nolabel, exec_arg)]
      in
      let success_fun =
        pexp_fun ~loc Nolabel None (ppat_construct ~loc (Located.mk ~loc (Lident "()")) None)
          (pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "bind"])
             [ (Nolabel, exec_call);
               ( Nolabel,
                 pexp_fun ~loc Nolabel None (ppat_construct ~loc (Located.mk ~loc (Lident "()")) None)
                   (pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "return"])
                      [ (Nolabel, econstr_ast ~loc "Ok" (Some (eunit_ast ~loc))) ]) ) ])
      in
      let error_msg =
        pexp_construct ~loc (Located.mk ~loc (Lident "Error")) (Some (eident_of_string ~loc "msg"))
      in
      let caqti_error_case =
        let rhs =
          pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "return"])
            [(Nolabel, pexp_construct ~loc (Located.mk ~loc (Lident "Error"))
              (Some (pexp_apply ~loc (epath_of_strings ~loc ["Caqti_error"; "show"])
                [(Nolabel, eident_of_string ~loc "error")])))]
        in
        case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Ldot (Lident "Caqti_error", "Exn")))
          (Some (pvar ~loc "error")))
          ~guard:None ~rhs
      in
      let generic_error_case =
        let rhs =
          pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "return"])
            [(Nolabel, pexp_construct ~loc (Located.mk ~loc (Lident "Error"))
              (Some (pexp_apply ~loc (epath_of_strings ~loc ["Printexc"; "to_string"])
                [(Nolabel, eident_of_string ~loc "exn")])))]
        in
        case ~lhs:(pvar ~loc "exn") ~guard:None ~rhs
      in
      let error_handler = pexp_function_cases ~loc [caqti_error_case; generic_error_case] in
      let lwt_catch =
        pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "catch"])
          [(Nolabel, success_fun); (Nolabel, error_handler)]
      in
      let body =
        if params = [] then lwt_catch
        else begin
          let empty_assoc =
            pexp_variant ~loc "Assoc" (Some (pexp_construct ~loc (Located.mk ~loc (Lident "[]")) None))
          in
          let payload_match =
            let some_p_case = case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some")) (Some (pvar ~loc "p")))
              ~guard:None ~rhs:(eident_of_string ~loc "p") in
            let none_case = case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "None")) None)
              ~guard:None ~rhs:empty_assoc in
            let assoc_inner =
              pexp_match ~loc
                (pexp_apply ~loc (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "List", "assoc_opt"))))
                  [(Nolabel, estring_of_string ~loc "payload"); (Nolabel, eident_of_string ~loc "fields")])
                [some_p_case; none_case]
            in
            let assoc_case = case ~lhs:(ppat_variant ~loc "Assoc" (Some (pvar ~loc "fields")))
              ~guard:None ~rhs:assoc_inner in
            let wild_case = case ~lhs:(ppat_any ~loc) ~guard:None ~rhs:empty_assoc in
            pexp_match ~loc (eident_of_string ~loc "action") [assoc_case; wild_case]
          in
          let payload_vb = value_binding ~loc ~pat:(pvar ~loc "payload") ~expr:payload_match in
          let param_vbs =
            params
            |> List.map (fun (param : query_param) ->
              let key = Option.value param.payload_key ~default:(Printf.sprintf "param_%d" param.index) in
              let var_name = sanitize_identifier key in
              let msg_binding =
                value_binding ~loc ~pat:(pvar ~loc "msg")
                  ~expr:(estring_of_string ~loc (Printf.sprintf "Missing or invalid field: %s" key))
              in
              let extractor_cases = json_extractor_cases_ast ~loc ~ocaml_type:param.ocaml_type in
              let assoc_inner =
                pexp_match ~loc
                  (pexp_apply ~loc (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "List", "assoc_opt"))))
                    [(Nolabel, estring_of_string ~loc key); (Nolabel, eident_of_string ~loc "fields")])
                  extractor_cases
              in
              let assoc_case = case ~lhs:(ppat_variant ~loc "Assoc" (Some (pvar ~loc "fields")))
                ~guard:None ~rhs:assoc_inner in
              let wild_case = case ~lhs:(ppat_any ~loc) ~guard:None
                ~rhs:(pexp_construct ~loc (Located.mk ~loc (Lident "Error")) (Some (eident_of_string ~loc "msg"))) in
              let param_match =
                pexp_match ~loc (eident_of_string ~loc "payload") [assoc_case; wild_case]
              in
              let param_body = pexp_let ~loc Nonrecursive [msg_binding] param_match in
              value_binding ~loc ~pat:(pvar ~loc var_name) ~expr:param_body)
          in
          let rec build_nested_matches = function
            | [] -> lwt_catch
            | v :: rest ->
                let ok_pat = ppat_construct ~loc (Located.mk ~loc (Lident "Ok")) (Some (pvar ~loc v)) in
                let ok_body = build_nested_matches rest in
                let ok_case = case ~lhs:ok_pat ~guard:None ~rhs:ok_body in
                let error_case = case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Error")) (Some (pvar ~loc "msg")))
                  ~guard:None
                  ~rhs:(pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "return"]) [(Nolabel, error_msg)]) in
                pexp_match ~loc (eident_of_string ~loc v) [error_case; ok_case]
          in
          let match_body = build_nested_matches var_names in
          let with_params =
            List.fold_right
              (fun vb acc -> pexp_let ~loc Nonrecursive [vb] acc)
              param_vbs match_body
          in
          pexp_let ~loc Nonrecursive [payload_vb] with_params
        end
      in
      let db_pat =
        ppat_constraint ~loc
          (ppat_unpack ~loc (Located.mk ~loc (Some "Db")))
          (ptyp_package ~loc (Located.mk ~loc (Ldot (Lident "Caqti_lwt", "CONNECTION")), []))
      in
      let dispatch_expr =
        pexp_fun ~loc Nolabel None db_pat
          (pexp_fun ~loc Nolabel None (pvar ~loc "action") body)
      in
      let platform_attr =
        attribute ~loc
          ~name:(Located.mk ~loc "platform")
          ~payload:(PStr [pstr_eval ~loc (eident_of_string ~loc "native") []])
      in
      let vb = value_binding ~loc ~pat:(pvar ~loc "dispatch") ~expr:dispatch_expr in
      let vb = { vb with pvb_attributes = [platform_attr] } in
      Some (pstr_value ~loc Nonrecursive [vb])

let mutation_module_declaration_ast ~loc (mutation : mutation) : structure_item list =
  let mod_name = module_name_of_table mutation.name in
  let params_metadata_expr =
    elist_of_exprs ~loc
      (List.map
         (fun (param : query_param) ->
           pexp_tuple ~loc
             [eint_of_int ~loc param.index; estring_of_string ~loc param.ocaml_type; estring_of_string ~loc param.sql_type])
         mutation.params)
  in
  let handler_expr = string_of_handler_ast ~loc mutation.handler in
  let param_type_expr = mutation_param_type_expr_ast ~loc mutation.params in
  let request_expr =
    let request_builder =
      pexp_open ~loc
        (open_infos ~loc ~expr:(mpath_of_strings ~loc ["Caqti_request"; "Infix"]) ~override:Fresh)
        (pexp_apply ~loc (eident_of_string ~loc "->.")
           [ (Nolabel, eident_of_string ~loc "param_type");
             (Nolabel, epath_of_strings ~loc ["Caqti_type"; "unit"]) ])
    in
    pexp_apply ~loc request_builder [(Nolabel, eident_of_string ~loc "sql")]
  in
  let name_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "name") ~expr:(estring_of_string ~loc mutation.name) ]
  in
  let sql_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "sql") ~expr:(estring_of_string ~loc mutation.sql) ]
  in
  let params_metadata_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "params_metadata") ~expr:params_metadata_expr ]
  in
  let handler_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc "handler") ~expr:handler_expr ]
  in
  let param_type_binding = native_value_binding ~loc "param_type" param_type_expr in
  let request_binding = native_value_binding ~loc "request" request_expr in
  let exec_binding =
    native_value_binding ~loc "exec"
      (pexp_fun ~loc Nolabel None (db_connection_pattern ~loc)
         (pexp_fun ~loc Nolabel None (pvar ~loc "param_values")
            (pexp_apply ~loc (epath_of_strings ~loc ["Lwt"; "bind"])
               [ (Nolabel, pexp_apply ~loc (epath_of_strings ~loc ["Db"; "exec"])
                             [ (Nolabel, eident_of_string ~loc "request");
                               (Nolabel, eident_of_string ~loc "param_values") ]);
                 (Nolabel, epath_of_strings ~loc ["Caqti_lwt"; "or_fail"]) ])))
  in
  let params_type_binding = params_type_declaration_ast ~loc mutation.params in
  let encode_params_binding = encode_params_code_ast ~loc mutation.params in
  let dispatch_items =
    match dispatch_function_for_mutation_ast ~loc mutation with
    | Some item -> [item]
    | None -> []
  in
  let structure_items = [
    name_binding;
    sql_binding;
    params_metadata_binding;
    handler_binding;
    param_type_binding;
    request_binding;
    exec_binding;
    params_type_binding;
    encode_params_binding;
  ] @ dispatch_items in
  [ pstr_module ~loc
      (module_binding ~loc
         ~name:(Located.mk ~loc (Some mod_name))
         ~expr:(pmod_structure ~loc structure_items))
  ]

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


let dispatch_mutation_ast ~loc (mutations : mutation list) : structure_item =
  let cases =
    mutations
    |> List.filter (fun (m : mutation) -> m.handler = Sql)
    |> List.map (fun (m : mutation) ->
           case ~lhs:(ppat_constant ~loc (Pconst_string (m.name, loc, None))) ~guard:None
             ~rhs:(esome_ast ~loc
                     (pexp_apply ~loc
                        (epath_of_strings ~loc ["Mutations"; module_name_of_table m.name; "dispatch"])
                        [ (Nolabel, pexp_pack ~loc (mpath_of_strings ~loc ["Db"]));
                          (Nolabel, eident_of_string ~loc "action") ])))
  in
  let default_case = case ~lhs:(ppat_any ~loc) ~guard:None ~rhs:(enone_ast ~loc) in
  let expr =
    pexp_fun ~loc Nolabel None (db_connection_pattern ~loc)
      (pexp_fun ~loc (Labelled "mutation_name") None (pvar ~loc "mutation_name")
         (pexp_fun ~loc Nolabel None (pvar ~loc "action")
            (pexp_match ~loc (eident_of_string ~loc "mutation_name") (cases @ [default_case]))))
  in
  native_value_binding ~loc "dispatch_mutation" expr



let pcd_simple ~loc name =
  constructor_declaration ~loc
    ~name:(Located.mk ~loc name)
    ~args:(Pcstr_tuple [])
    ~res:None

let pcd_with_arg ~loc name arg_type =
  constructor_declaration ~loc
    ~name:(Located.mk ~loc name)
    ~args:(Pcstr_tuple [arg_type])
    ~res:None

let toption ~loc inner =
  ptyp_constr ~loc (Located.mk ~loc (Lident "option")) [inner]

let tlist ~loc inner =
  ptyp_constr ~loc (Located.mk ~loc (Lident "list")) [inner]

let tstring ~loc = ptyp_constr ~loc (Located.mk ~loc (Lident "string")) []
let tint ~loc = ptyp_constr ~loc (Located.mk ~loc (Lident "int")) []
let tbool ~loc = ptyp_constr ~loc (Located.mk ~loc (Lident "bool")) []
let tident ~loc name = ptyp_constr ~loc (Located.mk ~loc (Lident name)) []
let tpair ~loc = ptyp_tuple ~loc [tstring ~loc; tstring ~loc]

let mutation_payload_type_name (mutation_name : string) : string =
  let pascal = module_name_of_table mutation_name in
  let first = String.sub pascal 0 1 |> String.lowercase_ascii in
  let rest = String.sub pascal 1 (String.length pascal - 1) in
  first ^ rest ^ "Params"

let mutation_payload_type_declaration_ast ~loc (mutation : mutation) : structure_item option =
  let payload_params = mutation.params
    |> List.filter (fun (p : query_param) -> p.payload_key <> None)
  in
  match List.length payload_params with
  | 0 | 1 -> None
  | _ ->
    let type_name = mutation_payload_type_name mutation.name in
    let label_decls = payload_params
      |> List.map (fun (param : query_param) ->
        let field_name = match param.payload_key with
          | Some key -> sanitize_identifier key
          | None -> Printf.sprintf "param_%d" param.index
        in
        label_declaration ~loc
          ~name:(Located.mk ~loc field_name)
          ~mutable_:Immutable
          ~type_:(ptyp_constr ~loc (Located.mk ~loc (Lident param.ocaml_type)) []))
    in
    Some (pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc type_name)
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record label_decls)
    ])

let mutation_payload_type_declarations_ast ~loc (mutations : mutation list) : structure_item list =
  mutations
  |> List.filter_map (mutation_payload_type_declaration_ast ~loc)

let action_variant_declaration_ast ~loc (mutations : mutation list) : structure_item =
  let constructors = mutations
    |> List.map (fun (mutation : mutation) ->
      let constructor_name = module_name_of_table mutation.name in
      let payload_params = mutation.params
        |> List.filter (fun (p : query_param) -> p.payload_key <> None)
      in
      match List.length payload_params with
      | 0 -> pcd_simple ~loc constructor_name
      | 1 ->
        let param = List.hd payload_params in
        pcd_with_arg ~loc constructor_name (ptyp_constr ~loc (Located.mk ~loc (Lident param.ocaml_type)) [])
      | _ ->
        let type_name = mutation_payload_type_name mutation.name in
        pcd_with_arg ~loc constructor_name (tident ~loc type_name)
    )
  in
  let custom_constructor =
    pcd_with_arg ~loc "Custom"
      (ptyp_var ~loc "custom")
  in
  let all_constructors = constructors @ [custom_constructor] in
  pstr_type ~loc Nonrecursive [
    type_declaration ~loc
      ~name:(Located.mk ~loc "action")
      ~params:[(ptyp_var ~loc "custom", (NoVariance, NoInjectivity))]
      ~cstrs:[]
      ~private_:Public
      ~manifest:None
      ~kind:(Ptype_variant all_constructors)
  ]

let payload_to_json_function_ast ~loc (mutation : mutation) : structure_item option =
  let payload_params = mutation.params
    |> List.filter (fun (p : query_param) -> p.payload_key <> None)
  in
  match List.length payload_params with
  | 0 | 1 -> None
  | _ ->
    let type_name = mutation_payload_type_name mutation.name in
    let fn_name = type_name ^ "_to_json" in
    let dict_binding =
      value_binding ~loc ~pat:(pvar ~loc "dict")
        ~expr:(pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "empty"]) [(Nolabel, eunit_ast ~loc)])
    in
    let set_calls =
      payload_params
      |> List.map (fun (param : query_param) ->
             let field_name = match param.payload_key with
               | Some key -> sanitize_identifier key
               | None -> Printf.sprintf "param_%d" param.index
             in
             let json_fn = epath_of_strings ~loc ["Melange_json"; "Primitives"; ocaml_type_to_json_fn param.ocaml_type] in
             pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "set"])
               [ (Nolabel, eident_of_string ~loc "dict");
                 (Nolabel, estring_of_string ~loc field_name);
                 ( Nolabel,
                   pexp_apply ~loc json_fn
                     [ (Nolabel, pexp_field ~loc (eident_of_string ~loc "p") (Located.mk ~loc (Lident field_name))) ] ) ])
    in
    let body =
      List.fold_right
        (fun call acc -> pexp_sequence ~loc call acc)
        set_calls
        (pexp_apply ~loc (epath_of_strings ~loc ["Melange_json"; "declassify"])
           [ ( Nolabel,
               json_assoc_expr ~loc
                 (pexp_apply ~loc (epath_of_strings ~loc ["Array"; "to_list"])
                    [ ( Nolabel,
                        pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "entries"])
                          [(Nolabel, eident_of_string ~loc "dict")] ) ]) ) ])
    in
    let param_pat =
      ppat_constraint ~loc (pvar ~loc "p")
        (ptyp_constr ~loc (Located.mk ~loc (Lident type_name)) [])
    in
    Some (pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc fn_name)
          ~expr:(pexp_fun ~loc Nolabel None param_pat
                   (pexp_let ~loc Nonrecursive [dict_binding] body)) ])

let payload_of_json_function_ast ~loc (mutation : mutation) : structure_item option =
  let payload_params = mutation.params
    |> List.filter (fun (p : query_param) -> p.payload_key <> None)
  in
  match List.length payload_params with
  | 0 | 1 -> None
  | _ ->
    let type_name = mutation_payload_type_name mutation.name in
    let fn_name = type_name ^ "_of_json" in
    let fields =
      payload_params
      |> List.map (fun (param : query_param) ->
             let field_name = match param.payload_key with
               | Some key -> sanitize_identifier key
               | None -> Printf.sprintf "param_%d" param.index
             in
             let of_json_fn = epath_of_strings ~loc ["Melange_json"; "Primitives"; ocaml_type_of_json_fn param.ocaml_type] in
             let decode_arg =
               pexp_fun ~loc Nolabel None (pvar ~loc "v") (pexp_apply ~loc of_json_fn [(Nolabel, eident_of_string ~loc "v")])
             in
             (Located.mk ~loc (Lident field_name),
              pexp_apply ~loc (epath_of_strings ~loc ["StoreJson"; "requiredField"])
                [ (Labelled "json", eident_of_string ~loc "p");
                  (Labelled "fieldName", estring_of_string ~loc field_name);
                  (Labelled "decode", decode_arg) ]))
    in
    let param_pat =
      ppat_constraint ~loc (pvar ~loc "p")
        (ptyp_constr ~loc (Located.mk ~loc (Lident type_name)) [])
    in
    let record_expr = pexp_record ~loc fields None in
    Some (pstr_value ~loc Nonrecursive
      [ value_binding ~loc ~pat:(pvar ~loc fn_name)
          ~expr:(pexp_fun ~loc Nolabel None param_pat record_expr) ])

let mutation_payload_params (mutation : mutation) : query_param list =
  mutation.params |> List.filter (fun (p : query_param) -> p.payload_key <> None)

let action_to_json_function_ast ~loc (mutations : mutation list) : structure_item =
  let cases =
    mutations
    |> List.map (fun (mutation : mutation) ->
           let constructor_name = module_name_of_table mutation.name in
           let kind_string = mutation.name in
           let payload_params = mutation_payload_params mutation in
           let num_params = List.length payload_params in
           let payload_expr =
             match num_params with
              | 0 ->
                pexp_apply ~loc (epath_of_strings ~loc ["Melange_json"; "declassify"])
                  [ (Nolabel, json_assoc_expr ~loc
                       (elist_of_exprs ~loc [])) ]
               | 1 ->
                 let param = List.hd payload_params in
                 let field_name = match param.payload_key with Some key -> key | None -> "param_0" in
                 let dict_binding =
                   value_binding ~loc ~pat:(pvar ~loc "dict")
                     ~expr:(pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "empty"]) [(Nolabel, eunit_ast ~loc)])
                 in
                 let json_fn = epath_of_strings ~loc ["Melange_json"; "Primitives"; ocaml_type_to_json_fn param.ocaml_type] in
                 let set_field =
                   pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "set"])
                     [ (Nolabel, eident_of_string ~loc "dict");
                       (Nolabel, estring_of_string ~loc field_name);
                       (Nolabel, pexp_apply ~loc json_fn [(Nolabel, eident_of_string ~loc "p")]) ]
                 in
                 let result =
                   pexp_apply ~loc (epath_of_strings ~loc ["Melange_json"; "declassify"])
                     [ (Nolabel, json_assoc_expr ~loc
                          (pexp_apply ~loc (epath_of_strings ~loc ["Array"; "to_list"])
                             [ (Nolabel,
                                 pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "entries"])
                                   [(Nolabel, eident_of_string ~loc "dict")] ) ])) ]
                 in
                 pexp_let ~loc Nonrecursive [dict_binding]
                   (pexp_sequence ~loc set_field result)
              | _ ->
                let type_name = mutation_payload_type_name mutation.name in
                pexp_apply ~loc (epath_of_strings ~loc [type_name ^ "_to_json"])
                  [(Nolabel, eident_of_string ~loc "p")]
            in
            let dict_binding =
             value_binding ~loc ~pat:(pvar ~loc "dict")
               ~expr:(pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "empty"]) [(Nolabel, eunit_ast ~loc)])
           in
           let set_kind =
             pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "set"])
               [ (Nolabel, eident_of_string ~loc "dict");
                 (Nolabel, estring_of_string ~loc "kind");
                 (Nolabel, pexp_apply ~loc (epath_of_strings ~loc ["Melange_json"; "Primitives"; "string_to_json"])
                    [(Nolabel, estring_of_string ~loc kind_string)]) ]
           in
           let set_payload =
             pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "set"])
               [ (Nolabel, eident_of_string ~loc "dict");
                 (Nolabel, estring_of_string ~loc "payload");
                 (Nolabel, payload_expr) ]
           in
           let result_expr =
             pexp_apply ~loc (epath_of_strings ~loc ["Melange_json"; "declassify"])
               [ (Nolabel, json_assoc_expr ~loc
                    (pexp_apply ~loc (epath_of_strings ~loc ["Array"; "to_list"])
                       [ (Nolabel,
                           pexp_apply ~loc (epath_of_strings ~loc ["Js"; "Dict"; "entries"])
                             [(Nolabel, eident_of_string ~loc "dict")] ) ])) ]
           in
           let body =
             pexp_let ~loc Nonrecursive [dict_binding]
               (pexp_sequence ~loc set_kind
                  (pexp_sequence ~loc set_payload result_expr))
           in
           let pat = match num_params with
             | 0 -> ppat_construct ~loc (Located.mk ~loc (Lident constructor_name)) None
             | _ -> ppat_construct ~loc (Located.mk ~loc (Lident constructor_name)) (Some (pvar ~loc "p"))
           in
           case ~lhs:pat ~guard:None ~rhs:body)
  in
  let custom_case =
    case ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Custom")) (Some (pvar ~loc "action")))
      ~guard:None
      ~rhs:(pexp_apply ~loc (eident_of_string ~loc "custom_to_json")
              [(Labelled "action", eident_of_string ~loc "action")])
  in
  let fun_body = pexp_match ~loc (eident_of_string ~loc "action") (cases @ [custom_case]) in
  let fun_expr =
    pexp_fun ~loc (Labelled "custom_to_json") None (pvar ~loc "custom_to_json")
      (pexp_fun ~loc Nolabel None (pvar ~loc "action") fun_body)
  in
  pstr_value ~loc Nonrecursive
    [ value_binding ~loc ~pat:(pvar ~loc "action_to_json") ~expr:fun_expr ]

let action_of_json_function_ast ~loc (mutations : mutation list) : structure_item =
  let string_of_json_fn = epath_of_strings ~loc ["Melange_json"; "Primitives"; "string_of_json"] in
  let decode_string_arg =
    pexp_fun ~loc Nolabel None (pvar ~loc "v")
      (pexp_apply ~loc string_of_json_fn [(Nolabel, eident_of_string ~loc "v")])
  in
  let decode_identity_arg =
    pexp_fun ~loc Nolabel None (pvar ~loc "v") (eident_of_string ~loc "v")
  in
  let kind_binding =
    value_binding ~loc ~pat:(pvar ~loc "kind")
      ~expr:(pexp_apply ~loc (epath_of_strings ~loc ["StoreJson"; "requiredField"])
               [ (Labelled "json", eident_of_string ~loc "json");
                 (Labelled "fieldName", estring_of_string ~loc "kind");
                 (Labelled "decode", decode_string_arg) ])
  in
  let payload_binding =
    value_binding ~loc ~pat:(pvar ~loc "payload")
      ~expr:(pexp_apply ~loc (epath_of_strings ~loc ["StoreJson"; "requiredField"])
               [ (Labelled "json", eident_of_string ~loc "json");
                 (Labelled "fieldName", estring_of_string ~loc "payload");
                 (Labelled "decode", decode_identity_arg) ])
  in
  let cases =
    mutations
    |> List.map (fun (mutation : mutation) ->
           let constructor_name = module_name_of_table mutation.name in
           let kind_string = mutation.name in
           let payload_params = mutation_payload_params mutation in
           let num_params = List.length payload_params in
            let rhs = match num_params with
              | 0 ->
                pexp_construct ~loc (Located.mk ~loc (Lident constructor_name)) None
               | 1 ->
                 let param = List.hd payload_params in
                 let field_name = match param.payload_key with Some key -> key | None -> "param_0" in
                 let of_json_fn = epath_of_strings ~loc ["Melange_json"; "Primitives"; ocaml_type_of_json_fn param.ocaml_type] in
                 let extract_field =
                   pexp_apply ~loc (epath_of_strings ~loc ["StoreJson"; "requiredField"])
                     [ (Labelled "json", eident_of_string ~loc "payload");
                       (Labelled "fieldName", estring_of_string ~loc field_name);
                       (Labelled "decode",
                         pexp_fun ~loc Nolabel None (pvar ~loc "v")
                           (pexp_apply ~loc of_json_fn [(Nolabel, eident_of_string ~loc "v")])) ]
                 in
                 pexp_construct ~loc (Located.mk ~loc (Lident constructor_name))
                   (Some extract_field)
              | _ ->
                let type_name = mutation_payload_type_name mutation.name in
               pexp_construct ~loc (Located.mk ~loc (Lident constructor_name))
                 (Some (pexp_apply ~loc (epath_of_strings ~loc [type_name ^ "_of_json"])
                          [(Nolabel, eident_of_string ~loc "payload")]))
           in
           case ~lhs:(ppat_constant ~loc (Pconst_string (kind_string, loc, None))) ~guard:None ~rhs)
    in
  let custom_rhs =
    pexp_construct ~loc (Located.mk ~loc (Lident "Custom"))
      (Some (pexp_apply ~loc (eident_of_string ~loc "custom_of_json")
               [(Labelled "kind", eident_of_string ~loc "kind");
                (Labelled "payload", eident_of_string ~loc "payload")]))
  in
  let default_case = case ~lhs:(ppat_any ~loc) ~guard:None ~rhs:custom_rhs in
  let switch_expr = pexp_match ~loc (eident_of_string ~loc "kind") (cases @ [default_case]) in
  let body = pexp_let ~loc Nonrecursive [kind_binding] (pexp_let ~loc Nonrecursive [payload_binding] switch_expr) in
  let fun_expr =
    pexp_fun ~loc (Labelled "custom_of_json") None (pvar ~loc "custom_of_json")
      (pexp_fun ~loc Nolabel None (pvar ~loc "json") body)
  in
  pstr_value ~loc Nonrecursive
    [ value_binding ~loc ~pat:(pvar ~loc "action_of_json") ~expr:fun_expr ]

let mutation_actions_module_ast ~loc (mutations : mutation list) : structure_item =
  let payload_decls = mutation_payload_type_declarations_ast ~loc mutations in
  let payload_to_json_helpers = mutations |> List.filter_map (payload_to_json_function_ast ~loc) in
  let payload_of_json_helpers = mutations |> List.filter_map (payload_of_json_function_ast ~loc) in
  let action_decl = action_variant_declaration_ast ~loc mutations in
  let action_to_json = action_to_json_function_ast ~loc mutations in
  let action_of_json = action_of_json_function_ast ~loc mutations in
  let items = payload_decls @ payload_to_json_helpers @ payload_of_json_helpers @ [action_decl; action_to_json; action_of_json] in
  pstr_module ~loc
    (module_binding ~loc
       ~name:(Located.mk ~loc (Some "MutationActions"))
       ~expr:(pmod_structure ~loc items))

let module_source_ast (schema : schema) ~loc : Parsetree.structure =
  (* --- Type declarations --- *)

  (* type sql_type = Uuid | Varchar | Text | ... | Custom of string *)
  let sql_type_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "sql_type")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_variant [
          pcd_simple ~loc "Uuid";
          pcd_simple ~loc "Varchar";
          pcd_simple ~loc "Text";
          pcd_simple ~loc "Int";
          pcd_simple ~loc "Bigint";
          pcd_simple ~loc "Boolean";
          pcd_simple ~loc "Timestamp";
          pcd_simple ~loc "Timestamptz";
          pcd_simple ~loc "Json";
          pcd_simple ~loc "Jsonb";
          pcd_with_arg ~loc "Custom" (tstring ~loc);
        ])
    ]
  in

  (* type foreign_key = { column : string; referenced_table : string; referenced_column : string; } *)
  let foreign_key_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "foreign_key")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record [
          label_declaration ~loc ~name:(Located.mk ~loc "column") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "referenced_table") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "referenced_column") ~mutable_:Immutable ~type_:(tstring ~loc);
        ])
    ]
  in

  (* type broadcast_channel = Column of string | Computed of string | Conditional of string | Subquery of string *)
  let broadcast_channel_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "broadcast_channel")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_variant [
          pcd_with_arg ~loc "Column" (tstring ~loc);
          pcd_with_arg ~loc "Computed" (tstring ~loc);
          pcd_with_arg ~loc "Conditional" (tstring ~loc);
          pcd_with_arg ~loc "Subquery" (tstring ~loc);
        ])
    ]
  in

  (* type broadcast_parent = { parent_table : string; query_name : string; } *)
  let broadcast_parent_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "broadcast_parent")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record [
          label_declaration ~loc ~name:(Located.mk ~loc "parent_table") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "query_name") ~mutable_:Immutable ~type_:(tstring ~loc);
        ])
    ]
  in

  (* type broadcast_to_views = { view_table : string; channel_column : string; } *)
  let broadcast_to_views_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "broadcast_to_views")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record [
          label_declaration ~loc ~name:(Located.mk ~loc "view_table") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "channel_column") ~mutable_:Immutable ~type_:(tstring ~loc);
        ])
    ]
  in

  (* type column_metadata = { ... } *)
  let column_metadata_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "column_metadata")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record [
          label_declaration ~loc ~name:(Located.mk ~loc "name") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "sql_type") ~mutable_:Immutable ~type_:(tident ~loc "sql_type");
          label_declaration ~loc ~name:(Located.mk ~loc "sql_type_raw") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "nullable") ~mutable_:Immutable ~type_:(tbool ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "primary_key") ~mutable_:Immutable ~type_:(tbool ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "default") ~mutable_:Immutable ~type_:(toption ~loc (tstring ~loc));
          label_declaration ~loc ~name:(Located.mk ~loc "foreign_key") ~mutable_:Immutable ~type_:(toption ~loc (tident ~loc "foreign_key"));
          label_declaration ~loc ~name:(Located.mk ~loc "definition_sql") ~mutable_:Immutable ~type_:(tstring ~loc);
        ])
    ]
  in

  (* type table_metadata = { ... } *)
  let table_metadata_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "table_metadata")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record [
          label_declaration ~loc ~name:(Located.mk ~loc "name") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "columns") ~mutable_:Immutable ~type_:(tlist ~loc (tident ~loc "column_metadata"));
          label_declaration ~loc ~name:(Located.mk ~loc "id_column") ~mutable_:Immutable ~type_:(toption ~loc (tstring ~loc));
          label_declaration ~loc ~name:(Located.mk ~loc "composite_key") ~mutable_:Immutable ~type_:(tlist ~loc (tstring ~loc));
          label_declaration ~loc ~name:(Located.mk ~loc "broadcast_channel") ~mutable_:Immutable ~type_:(toption ~loc (tident ~loc "broadcast_channel"));
          label_declaration ~loc ~name:(Located.mk ~loc "broadcast_parent") ~mutable_:Immutable ~type_:(toption ~loc (tident ~loc "broadcast_parent"));
          label_declaration ~loc ~name:(Located.mk ~loc "broadcast_to_views") ~mutable_:Immutable ~type_:(toption ~loc (tident ~loc "broadcast_to_views"));
          label_declaration ~loc ~name:(Located.mk ~loc "create_sql") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "source_file") ~mutable_:Immutable ~type_:(tstring ~loc);
        ])
    ]
  in

  (* type handler = Sql | Ocaml *)
  let handler_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "handler")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_variant [
          pcd_simple ~loc "Sql";
          pcd_simple ~loc "Ocaml";
        ])
    ]
  in

  (* type query_param = { index : int; column_ref : (string * string) option; payload_key : string option; ocaml_type : string; sql_type : string; } *)
  let query_param_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "query_param")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record [
          label_declaration ~loc ~name:(Located.mk ~loc "index") ~mutable_:Immutable ~type_:(tint ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "column_ref") ~mutable_:Immutable ~type_:(toption ~loc (tpair ~loc));
          label_declaration ~loc ~name:(Located.mk ~loc "payload_key") ~mutable_:Immutable ~type_:(toption ~loc (tstring ~loc));
          label_declaration ~loc ~name:(Located.mk ~loc "ocaml_type") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "sql_type") ~mutable_:Immutable ~type_:(tstring ~loc);
        ])
    ]
  in

  (* type query_metadata = { ... } *)
  let query_metadata_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "query_metadata")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record [
          label_declaration ~loc ~name:(Located.mk ~loc "name") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "sql") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "source_file") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "cache_key") ~mutable_:Immutable ~type_:(toption ~loc (tstring ~loc));
          label_declaration ~loc ~name:(Located.mk ~loc "return_table") ~mutable_:Immutable ~type_:(toption ~loc (tstring ~loc));
          label_declaration ~loc ~name:(Located.mk ~loc "json_columns") ~mutable_:Immutable ~type_:(tlist ~loc (tstring ~loc));
          label_declaration ~loc ~name:(Located.mk ~loc "params_metadata") ~mutable_:Immutable ~type_:(tlist ~loc (tident ~loc "query_param"));
          label_declaration ~loc ~name:(Located.mk ~loc "handler") ~mutable_:Immutable ~type_:(tident ~loc "handler");
        ])
    ]
  in

  (* type mutation_metadata = { ... } *)
  let mutation_metadata_decl =
    pstr_type ~loc Nonrecursive [
      type_declaration ~loc
        ~name:(Located.mk ~loc "mutation_metadata")
        ~params:[]
        ~cstrs:[]
        ~private_:Public
        ~manifest:None
        ~kind:(Ptype_record [
          label_declaration ~loc ~name:(Located.mk ~loc "name") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "sql") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "source_file") ~mutable_:Immutable ~type_:(tstring ~loc);
          label_declaration ~loc ~name:(Located.mk ~loc "params_metadata") ~mutable_:Immutable ~type_:(tlist ~loc (tident ~loc "query_param"));
          label_declaration ~loc ~name:(Located.mk ~loc "handler") ~mutable_:Immutable ~type_:(tident ~loc "handler");
        ])
    ]
  in

  (* Table record type declarations (from existing AST function) *)
  let table_type_decls = schema.tables |> List.map (record_type_declaration_ast ~loc) in

  (* --- Value bindings --- *)

  (* let schema_hash = "..." *)
  let schema_hash_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "schema_hash")
          ~expr:(estring_of_string ~loc schema.schema_hash) ]
  in

  (* let source_files = [...] *)
  let source_files_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "source_files")
          ~expr:(elist_of_exprs ~loc (List.map (estring_of_string ~loc) schema.source_files)) ]
  in

  (* let tables : table_metadata list = [...] *)
  let tables_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "tables")
          ~expr:(elist_of_exprs ~loc (List.map (table_record_literal_ast ~loc) schema.tables)) ]
  in

  (* let queries : query_metadata list = [...] *)
  let queries_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "queries")
          ~expr:(elist_of_exprs ~loc (List.map (query_record_literal_ast ~loc) schema.queries)) ]
  in

  (* let mutations : mutation_metadata list = [...] *)
  let mutations_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "mutations")
          ~expr:(elist_of_exprs ~loc (List.map (mutation_record_literal_ast ~loc) schema.mutations)) ]
  in

  (* let generated_triggers_sql = "..." *)
  let triggers_sql = generated_triggers_sql schema in
  let generated_triggers_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "generated_triggers_sql")
          ~expr:(estring_of_string ~loc triggers_sql) ]
  in

  (* let latest_migration_sql = "..." *)
  let _, migration_sql_str = migration_sql schema in
  let latest_migration_binding =
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "latest_migration_sql")
          ~expr:(estring_of_string ~loc migration_sql_str) ]
  in

  (* --- Function definitions --- *)

  (* let find_query name = List.find_opt (fun (query : query_metadata) -> query.name = name) queries *)
  let find_query_fn =
    let query_pat = ppat_constraint ~loc (pvar ~loc "query") (tident ~loc "query_metadata") in
    let pred =
      pexp_apply ~loc (eident_of_string ~loc "=")
        [ (Nolabel, pexp_field ~loc (eident_of_string ~loc "query") (Located.mk ~loc (Lident "name")));
          (Nolabel, eident_of_string ~loc "name") ]
    in
    let find_opt_call = pexp_apply ~loc
      (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "List", "find_opt"))))
      [ (Nolabel, pexp_fun ~loc Nolabel None query_pat pred);
        (Nolabel, eident_of_string ~loc "queries") ] in
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "find_query")
          ~expr:(pexp_fun ~loc Nolabel None (pvar ~loc "name") find_opt_call) ]
  in

  (* let find_mutation name = List.find_opt (fun (mutation : mutation_metadata) -> mutation.name = name) mutations *)
  let find_mutation_fn =
    let mutation_pat = ppat_constraint ~loc (pvar ~loc "mutation") (tident ~loc "mutation_metadata") in
    let pred =
      pexp_apply ~loc (eident_of_string ~loc "=")
        [ (Nolabel, pexp_field ~loc (eident_of_string ~loc "mutation") (Located.mk ~loc (Lident "name")));
          (Nolabel, eident_of_string ~loc "name") ]
    in
    let find_opt_call = pexp_apply ~loc
      (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "List", "find_opt"))))
      [ (Nolabel, pexp_fun ~loc Nolabel None mutation_pat pred);
        (Nolabel, eident_of_string ~loc "mutations") ] in
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "find_mutation")
          ~expr:(pexp_fun ~loc Nolabel None (pvar ~loc "name") find_opt_call) ]
  in

  (* let find_table name = List.find_opt (fun (table : table_metadata) -> table.name = name) tables *)
  let find_table_fn =
    let table_pat = ppat_constraint ~loc (pvar ~loc "table") (tident ~loc "table_metadata") in
    let pred =
      pexp_apply ~loc (eident_of_string ~loc "=")
        [ (Nolabel, pexp_field ~loc (eident_of_string ~loc "table") (Located.mk ~loc (Lident "name")));
          (Nolabel, eident_of_string ~loc "name") ]
    in
    let find_opt_call = pexp_apply ~loc
      (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "List", "find_opt"))))
      [ (Nolabel, pexp_fun ~loc Nolabel None table_pat pred);
        (Nolabel, eident_of_string ~loc "tables") ] in
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "find_table")
          ~expr:(pexp_fun ~loc Nolabel None (pvar ~loc "name") find_opt_call) ]
  in

  (* let find_column table_name column_name =
       match find_table table_name with
       | Some table -> List.find_opt (fun (column : column_metadata) -> column.name = column_name) table.columns
       | None -> None *)
  let find_column_fn =
    let col_pat = ppat_constraint ~loc (pvar ~loc "column") (tident ~loc "column_metadata") in
    let col_pred =
      pexp_apply ~loc (eident_of_string ~loc "=")
        [ (Nolabel, pexp_field ~loc (eident_of_string ~loc "column") (Located.mk ~loc (Lident "name")));
          (Nolabel, eident_of_string ~loc "column_name") ]
    in
    let col_find = pexp_apply ~loc
      (pexp_ident ~loc (Located.mk ~loc (Ldot (Lident "List", "find_opt"))))
      [ (Nolabel, pexp_fun ~loc Nolabel None col_pat col_pred);
        (Nolabel, pexp_field ~loc (eident_of_string ~loc "table") (Located.mk ~loc (Lident "columns"))) ] in
    let some_case = case
      ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some")) (Some (pvar ~loc "table")))
      ~guard:None
      ~rhs:col_find in
    let none_case = case
      ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "None")) None)
      ~guard:None
      ~rhs:(enone_ast ~loc) in
    let find_table_call = pexp_apply ~loc (eident_of_string ~loc "find_table") [(Nolabel, eident_of_string ~loc "table_name")] in
    let match_expr = pexp_match ~loc find_table_call [some_case; none_case] in
    let body = pexp_fun ~loc Nolabel None (pvar ~loc "column_name") match_expr in
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "find_column")
          ~expr:(pexp_fun ~loc Nolabel None (pvar ~loc "table_name") body) ]
  in

  (* let table_name name = match find_table name with Some table -> table.name | None -> name *)
  let table_name_fn =
    let some_case = case
      ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "Some")) (Some (pvar ~loc "table")))
      ~guard:None
      ~rhs:(pexp_field ~loc (eident_of_string ~loc "table") (Located.mk ~loc (Lident "name"))) in
    let none_case = case
      ~lhs:(ppat_construct ~loc (Located.mk ~loc (Lident "None")) None)
      ~guard:None
      ~rhs:(eident_of_string ~loc "name") in
    let find_table_call = pexp_apply ~loc (eident_of_string ~loc "find_table") [(Nolabel, eident_of_string ~loc "name")] in
    let match_expr = pexp_match ~loc find_table_call [some_case; none_case] in
    pstr_value ~loc Nonrecursive
      [ value_binding ~loc
          ~pat:(pvar ~loc "table_name")
          ~expr:(pexp_fun ~loc Nolabel None (pvar ~loc "name") match_expr) ]
  in

  (* --- Module declarations --- *)

  (* module Tables = struct ... end *)
  let tables_module_items =
    schema.tables |> List.map (table_module_declaration_ast ~loc) |> List.concat
  in
  let tables_module =
    pstr_module ~loc
      (module_binding ~loc
         ~name:(Located.mk ~loc (Some "Tables"))
         ~expr:(pmod_structure ~loc tables_module_items))
  in

  (* module Queries = struct ... end *)
  let queries_module_items =
    schema.queries |> List.map (query_module_declaration_ast ~loc schema.tables) |> List.concat
  in
  let queries_module =
    pstr_module ~loc
      (module_binding ~loc
         ~name:(Located.mk ~loc (Some "Queries"))
         ~expr:(pmod_structure ~loc queries_module_items))
  in

  (* module Mutations = struct ... end *)
  let mutations_module_items =
    schema.mutations |> List.map (mutation_module_declaration_ast ~loc) |> List.concat
  in
  let mutations_module =
    pstr_module ~loc
      (module_binding ~loc
         ~name:(Located.mk ~loc (Some "Mutations"))
         ~expr:(pmod_structure ~loc mutations_module_items))
  in

  let mutation_actions_module =
    mutation_actions_module_ast ~loc schema.mutations
  in

  let dispatch_mutation_binding = dispatch_mutation_ast ~loc schema.mutations in

  (* Assemble the inner structure *)
  let inner_items = [
    sql_type_decl;
    foreign_key_decl;
    broadcast_channel_decl;
    broadcast_parent_decl;
    broadcast_to_views_decl;
    column_metadata_decl;
    table_metadata_decl;
    handler_decl;
    query_param_decl;
    query_metadata_decl;
    mutation_metadata_decl;
  ] @ table_type_decls @ [
    schema_hash_binding;
    source_files_binding;
    tables_binding;
    queries_binding;
    mutations_binding;
    generated_triggers_binding;
    latest_migration_binding;
    find_query_fn;
    find_mutation_fn;
    find_table_fn;
    find_column_fn;
    table_name_fn;
    tables_module;
    queries_module;
    mutations_module;
    mutation_actions_module;
    dispatch_mutation_binding;
  ] in

  (* include struct ... end *)
  let incl : Parsetree.include_declaration = {
    pincl_mod = pmod_structure ~loc inner_items;
    pincl_loc = loc;
    pincl_attributes = [];
  } in
  [ { pstr_desc = Pstr_include incl; pstr_loc = loc } ]

let module_source (schema : schema) ~loc : Parsetree.structure =
  module_source_ast schema ~loc

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
