open Lwt.Syntax;

let writeEvent = (
  stream: Dream.stream,
  ~event: option(string)=?,
  ~id: option(string)=?,
  ~data: string,
  (),
) : Lwt.t(unit) => {
  let lines =
    if (String.length(data) == 0) {
      [||]
    } else {
      let rec split = (start, acc) => {
        let len = String.length(data);
        if (start >= len) {
          acc
        } else {
          let idx = ref(start);
          while (idx^ < len && data.[idx^] != '\n') {
            incr(idx);
          };
          let line = String.sub(data, start, idx^ - start);
          let newAcc = Js.Array.concat(~other=[|line|], acc);
          if (idx^ < len && data.[idx^] == '\n') {
            split(idx^ + 1, newAcc);
          } else {
            newAcc;
          };
        };
      };
      split(0, [||]);
    };

  let rec writeLines = (i) => {
    if (i >= Array.length(lines)) {
      Lwt.return_unit;
    } else {
      let* () = Dream.write(stream, "data: " ++ lines[i] ++ "\n");
      writeLines(i + 1);
    };
  };

  let* () = writeLines(0);

  let* () =
    switch (event) {
    | Some(e) => Dream.write(stream, "event: " ++ e ++ "\n")
    | None => Lwt.return_unit
    };

  let* () =
    switch (id) {
    | Some(i) => Dream.write(stream, "id: " ++ i ++ "\n")
    | None => Lwt.return_unit
    };

  let* () = Dream.write(stream, "\n");
  Dream.flush(stream);
};
