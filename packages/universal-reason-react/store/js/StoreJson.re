type json = Melange_json.t;

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
    let json = (Obj.magic(json): Melange_json.t);
    switch (Melange_json.classify(json)) {
    | `Assoc(entries) => {
        let dict = Js.Dict.empty();
        List.iter(
          ((key, value)) => dict->Js.Dict.set(key, decodeValue(Obj.magic(value))),
          entries,
        );
        dict;
      }
    | _ => Melange_json.of_json_error(~json, "expected object")
    };
  };

  [@platform js]
  let to_json = (encodeValue, dict) => Melange_json.To_json.js_dict(encodeValue)(dict);

  [@platform native]
  let to_json = (encodeValue, dict) => {
    let entries =
      List.fold_left(
        (entries, key) => {
          switch (dict->Js.Dict.get(key)) {
          | Some(value) => [(key, (Obj.magic(encodeValue(value)): Melange_json.t)), ...entries]
          | None => entries
          };
        },
        [],
        dict->Js.Dict.keys->Array.to_list,
      );

    Melange_json.declassify(`Assoc(List.rev(entries))) |> Obj.magic;
  };
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
