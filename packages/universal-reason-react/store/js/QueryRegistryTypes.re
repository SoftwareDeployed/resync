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

type loaded_query_result = {
  key: query_key,
  channel: string,
  rows: array(StoreJson.json),
};

let keySeparator = ':';

let makeKey = (~channel: string, ~paramsHash: string): query_key => {
  string_of_int(String.length(channel)) ++ ":" ++ channel ++ ":" ++ paramsHash;
};

let findLastSeparator = (key: query_key): option(int) => {
  let found = ref(None);
  for (index in 0 to String.length(key) - 1) {
    if (String.get(key, index) == keySeparator) {
      found := Some(index);
    };
  };
  found.contents;
};

let channelOfLegacyKey = (key: query_key): string => {
  switch (findLastSeparator(key)) {
  | Some(index) => String.sub(key, 0, index)
  | None => key
  };
};

let channelOfLengthPrefixedKey = (key: query_key): option(string) => {
  let keyLength = String.length(key);
  let firstSeparator =
    try(Some(String.index(key, keySeparator))) {
    | Not_found => None
    };
  switch (firstSeparator) {
  | None => None
  | Some(separator) =>
    let lengthText = String.sub(key, 0, separator);
    switch (int_of_string_opt(lengthText)) {
    | Some(channelLength) =>
      let channelStart = separator + 1;
      let separatorAfterChannel = channelStart + channelLength;
      if (
        channelLength >= 0
        && separatorAfterChannel < keyLength
        && String.get(key, separatorAfterChannel) == keySeparator
      ) {
        Some(String.sub(key, channelStart, channelLength));
      } else {
        None;
      }
    | None => None
    }
  };
};

let channelOfKey = (key: query_key): string => {
  switch (channelOfLengthPrefixedKey(key)) {
  | Some(channel) => channel
  | None => channelOfLegacyKey(key)
  };
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

// Low-level mutation contract for manual dispatch. `UseMutation.make`
// only needs the params type; callers provide the actual dispatch callback.
module type MutationModule = {
  type params;
};

// Action-aware mutation contract for store-scoped hooks that auto-dispatch.
// Adds `toAction` so the hook can construct a store action from params
// without the caller providing an explicit ~onDispatch callback.
module type MutationModuleWithAction = {
  type params;
  type action;
  let toAction: params => action;
};

// Native SSR uses the same marker contract.
module type MutationModuleNative = MutationModule;
