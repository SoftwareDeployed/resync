type json = Melange_json.t;

let rawOfJson = (json: json): Melange_json.t => json;
let jsonOfRaw = (json: Melange_json.t): json => json;

let listOfArray = (items: array('a)): list('a) => {
  let rec loop = (index, acc) =>
    if (index < 0) {
      acc;
    } else {
      loop(index - 1, [items[index], ...acc]);
    };

  loop(Array.length(items) - 1, []);
};

let reverseList = (items: list('a)): list('a) => {
  let rec loop = (remaining, acc) =>
    switch (remaining) {
    | [] => acc
    | [item, ...rest] => loop(rest, [item, ...acc])
    };

  loop(items, []);
};

module Date = {
  type t = Js.Date.t;

  let of_json = json => json->Melange_json.Of_json.string->Js.Date.fromString;
  let to_json = date => date->Js.Date.toJSONUnsafe->Melange_json.To_json.string;
};

module Dict = {
  type t('a) = Js.Dict.t('a);

  [@platform js]
  let of_json = (decodeValue, json) => Melange_json.Of_json.js_dict(decodeValue)(json);

  [@platform native]
  let of_json = (decodeValue, json) => {
    let rawJson = rawOfJson(json);
    switch (Melange_json.classify(rawJson)) {
    | `Assoc(entries) => {
        let dict = Js.Dict.empty();

        let rec addEntries = entries =>
          switch (entries) {
          | [] => dict
          | [(key, value), ...rest] =>
            dict->Js.Dict.set(key, decodeValue(jsonOfRaw(value)));
            addEntries(rest);
          };

        addEntries(entries);
      }
    | _ => Melange_json.of_json_error(~json=rawJson, "expected object")
    };
  };

  [@platform js]
  let to_json = (encodeValue, dict) => Melange_json.To_json.js_dict(encodeValue)(dict);

  [@platform native]
  let to_json = (encodeValue, dict) => {
    let keys = dict->Js.Dict.keys;
    let rec collectEntries = (index, entries) =>
      if (index >= Array.length(keys)) {
        reverseList(entries);
      } else {
        let key = keys[index];
        switch (dict->Js.Dict.get(key)) {
        | Some(value) =>
          collectEntries(index + 1, [(key, rawOfJson(encodeValue(value))), ...entries])
        | None => collectEntries(index + 1, entries)
        };
      };

    Melange_json.declassify(`Assoc(collectEntries(0, []))) |> jsonOfRaw;
  };
};

module Object = {
  type t = Js.Dict.t(json);

  let make = fill => {
    let dict: t = Js.Dict.empty();
    fill(dict);
    Dict.to_json(json => json, dict);
  };

  let setJson = (dict: t, key, value) => dict->Js.Dict.set(key, value);
  let setString = (dict: t, key, value) =>
    setJson(dict, key, Melange_json.Primitives.string_to_json(value));
  let setInt = (dict: t, key, value) =>
    setJson(dict, key, Melange_json.Primitives.int_to_json(value));
  let setFloat = (dict: t, key, value) =>
    setJson(dict, key, Melange_json.Primitives.float_to_json(value));
  let setBool = (dict: t, key, value) =>
    setJson(dict, key, Melange_json.Primitives.bool_to_json(value));
};

let parse = (data: string) => Melange_json.of_string(data);

let tryParse = (data: string) =>
  try(Some(parse(data))) {
  | _ => None
  };

let decodeString = (decode, data: string) => data->parse->decode;

let tryDecode = (decode, json) =>
  try(Some(decode(json))) {
  | _ => None
  };

let tryDecodeString = (decode, data: string) =>
  switch (tryParse(data)) {
  | Some(json) => tryDecode(decode, json)
  | None => None
  };

let stringify = (encode, value) => value->encode->Melange_json.to_string;

[@platform native]
let toSafe = (json: json): Yojson.Safe.t => {
  stringify(json => json, json)->Yojson.Safe.from_string;
};

[@platform native]
let ofSafe = (json: Yojson.Safe.t): json => {
  json->Yojson.Safe.to_string->parse;
};

[@platform native]
let safeListOfArray = (items: array(Yojson.Safe.t)): Yojson.Safe.t => {
  `List(listOfArray(items));
};

let field = (json, key) => {
  let rawJson = rawOfJson(json);
  switch (Melange_json.classify(rawJson)) {
  | `Assoc(entries) => {
      let rec find = entries =>
        switch (entries) {
        | [] => None
        | [(entryKey, value), ...rest] =>
          entryKey == key ? Some(jsonOfRaw(value)) : find(rest)
        };

      find(entries);
    }
  | _ => None
  };
};

let requiredField = (~json, ~fieldName, ~decode) =>
  switch (field(json, fieldName)) {
  | Some(value) => decode(value)
  | None => Js.Exn.raiseError("missing field: " ++ fieldName)
  };

let optionalField = (~json, ~fieldName, ~decode) =>
  switch (field(json, fieldName)) {
  | Some(value) => Some(decode(value))
  | None => None
  };

let decodeEmbedded = (~decode, json) => {
  let rawJson = rawOfJson(json);
  switch (Melange_json.classify(rawJson)) {
  | `String(encodedJson) => tryDecodeString(decode, encodedJson)
  | `Null => None
  | _ => tryDecode(decode, json)
  };
};

let decodeIntWithDefault = (~default=0, json) =>
  switch (tryDecode(Melange_json.Primitives.int_of_json, json)) {
  | Some(value) => value
  | None => default
  };
