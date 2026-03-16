type t =
  | Premise(string);

let premise = (premise_id: string): t => Premise(premise_id);

let is_hex_digit = (char: char) =>
  switch (char) {
  | '0' .. '9'
  | 'a' .. 'f'
  | 'A' .. 'F' => true
  | _ => false
  };

let is_uuid = (value: string) => {
  let length = String.length(value);
  if (Int.equal(length, 36)) {
    let rec loop = index =>
      if (Int.equal(index, length)) {
        true;
      } else {
        let char = String.get(value, index);
        if (
          Int.equal(index, 8)
          || Int.equal(index, 13)
          || Int.equal(index, 18)
          || Int.equal(index, 23)
        ) {
          Char.equal(char, '-') && loop(index + 1);
        } else {
          is_hex_digit(char) && loop(index + 1);
        };
      };
    loop(0);
  } else {
    false;
  };
};

let channel = (subscription: t): string =>
  switch (subscription) {
  | Premise(premise_id) => premise_id
  };

let encode = (subscription: t): string =>
  switch (subscription) {
  | Premise(premise_id) => "premise:" ++ premise_id
  };

let decode = (value: string): option(t) => {
  let prefix = "premise:";
  let prefix_length = String.length(prefix);
  let value_length = String.length(value);
  if (value_length <= prefix_length) {
    None;
  } else if (String.equal(String.sub(value, 0, prefix_length), prefix)) {
    let premise_id = String.sub(value, prefix_length, value_length - prefix_length);
    if (is_uuid(premise_id)) {
      Some(Premise(premise_id));
    } else {
      None;
    };
  } else {
    None;
  };
};

let decode_channel = (value: string): option(string) =>
  switch (decode(value)) {
  | Some(subscription) => Some(channel(subscription))
  | None => None
  };
