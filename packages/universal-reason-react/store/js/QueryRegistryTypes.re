// QueryRegistryTypes.re - Shared type definitions for query registration system

type query_key = string;

type query_spec('params, 'row) = {
  channel: string,
  params: 'params,
  sql: string,
  decodeRow: StoreJson.json => 'row,
};

type query_result('row) =
  | Loading
  | Loaded(array('row))
  | Error(string);

type registered_query('params, 'row) = {
  key: query_key,
  spec: query_spec('params, 'row),
  mutable result: query_result('row),
};

type registry_snapshot = {
  queries: array(query_key),
  results: Js.Dict.t(StoreJson.json),
};

let makeKey = (~channel: string, ~paramsHash: string): query_key => {
  channel ++ ":" ++ paramsHash;
};

// Module type for query modules used by useQuery hook
module type QueryModule = {
  type params;
  type row;
  let channel: params => string;
  let paramsHash: params => string;
  let decodeRow: StoreJson.json => row;
  let row_to_json: row => StoreJson.json;
  [@platform native]
  let execute:
    (module Caqti_lwt.CONNECTION) =>
    params =>
    Lwt.t(Stdlib.result(array(row), string));
};

// Module type for mutation modules used by useMutation hook
module type MutationModule = {
  type params;
  let encodeParams: params => StoreJson.json;
  let name: string;
};

// Module type for mutation modules on native (server-side SSR)
module type MutationModuleNative = {
  type params;
};
