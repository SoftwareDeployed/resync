type t = {buffer: ref(string)};

let make = () => {buffer: ref("")};

let feed = (parser: t, chunk: string) : array(Melange_json.t) => {
  parser.buffer := parser.buffer^ ++ chunk;
  let raw = parser.buffer^;

  let rec split = (start, acc) => {
    let len = String.length(raw);
    if (start >= len) {
      (acc, "")
    } else {
      let idx = ref(start);
      while (idx^ < len && raw.[idx^] != '\n') {
        incr(idx);
      };
      if (idx^ >= len) {
        (acc, String.sub(raw, start, len - start))
      } else {
        let line = String.sub(raw, start, idx^ - start);
        split(idx^ + 1, Js.Array.concat(~other=[|line|], acc));
      };
    };
  };

  let (lines, remainder) = split(0, [||]);

  let parsedLines =
    lines
    |> Js.Array.map(~f=(line => {
         if (String.length(line) == 0) {
           None
         } else {
           try(Some(Melange_json.of_string(line))) {
           | _ => None
           }
         }
       }))
    |> Js.Array.filter(~f=(opt => switch(opt) { | Some(_) => true | None => false }))
    |> Js.Array.map(~f=(opt => switch(opt) { | Some(v) => v | None => assert(false) }));

  if (String.length(remainder) > 0) {
    try({
      let json = Melange_json.of_string(remainder);
      parser.buffer := "";
      Js.Array.concat(~other=[|json|], parsedLines);
    }) {
    | _ => {
      parser.buffer := remainder;
      parsedLines;
    };
    };
  } else {
    parser.buffer := "";
    parsedLines;
  };
};
