let escape = (json: string): string => {
  let buffer = Buffer.create(String.length(json));

  for (index in 0 to String.length(json) - 1) {
    switch (String.get(json, index)) {
    | '<' => Buffer.add_string(buffer, "\\u003C")
    | '>' => Buffer.add_string(buffer, "\\u003E")
    | '&' => Buffer.add_string(buffer, "\\u0026")
    | char => Buffer.add_char(buffer, char)
    };
  };

  Buffer.contents(buffer);
};

/* Reverses `escape`. Needed when reading text/plain script contents (which
   are not parsed as JS, so \uXXXX sequences stay literal). */
let unescape = (value: string): string => {
  let length = String.length(value);
  let buffer = Buffer.create(length);
  let matchesAt = (index, seq) =>
    index + 6 <= length && String.sub(value, index, 6) == seq;
  let rec step = index =>
    if (index < length) {
      let consumed =
        if (matchesAt(index, "\\u003C")) {
          Buffer.add_string(buffer, "<");
          true;
        } else if (matchesAt(index, "\\u003E")) {
          Buffer.add_string(buffer, ">");
          true;
        } else if (matchesAt(index, "\\u0026")) {
          Buffer.add_string(buffer, "&");
          true;
        } else {
          Buffer.add_char(buffer, String.get(value, index));
          false;
        };
      step(index + (consumed ? 6 : 1));
    };
  step(0);
  Buffer.contents(buffer);
};
