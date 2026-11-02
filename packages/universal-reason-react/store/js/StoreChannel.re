/*
 * StoreChannel: helpers for prefixed UUID channel subscriptions.
 *
 * Realtime subscriptions follow the wire format "<prefix>:<uuid>", e.g.
 * "thread:<uuid>" or "premise:<uuid>". Demos previously hand-rolled this
 * logic (UUID validation, prefix encode/decode) in every
 * RealtimeSubscription module and server; it lives here instead.
 *
 * Works on both Melange (client) and native (server) targets.
 */

let isHexDigit = (char: char) =>
  switch (char) {
  | '0' .. '9'
  | 'a' .. 'f'
  | 'A' .. 'F' => true
  | _ => false
  };

let isValidUuid = (value: string) => {
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
          isHexDigit(char) && loop(index + 1);
        };
      };
    loop(0);
  } else {
    false;
  };
};

/* "thread" + id => "thread:<uuid>" */
let encode = (~prefix, ~id) => prefix ++ ":" ++ id;

/* "thread:<uuid>" => Some(<uuid>) when the id is a well-formed UUID */
let decodeId = (~prefix, ~value) => {
  let expectedPrefix = prefix ++ ":";
  let prefixLength = String.length(expectedPrefix);
  let valueLength = String.length(value);
  if (valueLength <= prefixLength) {
    None;
  } else if (String.equal(String.sub(value, 0, prefixLength), expectedPrefix)) {
    let id = String.sub(value, prefixLength, valueLength - prefixLength);
    isValidUuid(id) ? Some(id) : None;
  } else {
    None;
  };
};
