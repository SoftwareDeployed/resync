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
