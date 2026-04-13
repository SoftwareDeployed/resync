type sql_type =
  | Uuid
  | Varchar
  | Text
  | Int
  | Bigint
  | Boolean
  | Timestamp
  | Timestamptz
  | Json
  | Jsonb
  | Custom of string

type foreign_key = {
  column : string;
  referenced_table : string;
  referenced_column : string;
}

type column = {
  name : string;
  sql_type : sql_type;
  sql_type_raw : string;
  nullable : bool;
  primary_key : bool;
  default : string option;
  foreign_key : foreign_key option;
  definition_sql : string;
}

type broadcast_channel =
  | Column of string
  | Computed of string
  | Conditional of string
  | Subquery of string

type broadcast_parent = {
  parent_table : string;
  query_name : string;
}

type broadcast_to_views = {
  view_table : string;
  channel_column : string;
}

type table = {
  name : string;
  columns : column list;
  id_column : string option;
  composite_key : string list;
  broadcast_channel : broadcast_channel option;
  broadcast_parent : broadcast_parent option;
  broadcast_to_views : broadcast_to_views option;
  create_sql : string;
  source_file : string;
}

type handler =
  | Sql
  | Ocaml

type query_param = {
  index : int;
  column_ref : (string * string) option;
  payload_key : string option;
  ocaml_type : string;
  sql_type : string;
}

type query = {
  name : string;
  sql : string;
  source_file : string;
  cache_key : string option;
  return_table : string option;
  json_columns : string list;
  params : query_param list;
  handler : handler;
}

type mutation = {
  name : string;
  sql : string;
  source_file : string;
  params : query_param list;
  handler : handler;
}

type schema = {
  tables : table list;
  queries : query list;
  mutations : mutation list;
  source_files : string list;
  schema_hash : string;
}

type table_snapshot = {
  table_name : string;
  columns : column list;
  id_column : string option;
  composite_key : string list;
}

type snapshot = {
  schema_hash : string;
  tables : table_snapshot list;
}
