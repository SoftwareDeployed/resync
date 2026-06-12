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
