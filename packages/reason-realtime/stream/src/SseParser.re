type event = {
  id: option(string),
  event: option(string),
  data: string,
};

let parseChunk = (chunk: string, ~buffer: ref(string)) : array(event) => {
  buffer := buffer^ ++ chunk;
  let raw = buffer^;

  let rec findBoundaries = (start, acc) => {
    let len = String.length(raw);
    if (start >= len) {
      (acc, "")
    } else {
      let idx = ref(start);
      while (idx^ < len && !(raw.[idx^] == '\n' && idx^ + 1 < len && raw.[idx^ + 1] == '\n')) {
        incr(idx);
      };
      if (idx^ >= len) {
        (acc, String.sub(raw, start, len - start))
      } else {
        let block = String.sub(raw, start, idx^ - start);
        findBoundaries(idx^ + 2, Js.Array.concat(~other=[|block|], acc));
      };
    };
  };

  let (blocks, remainder) = findBoundaries(0, [||]);
  buffer := remainder;

  blocks
  |> Js.Array.map(~f=(block => {
       let lines =
         if (String.length(block) == 0) {
           [||]
         } else {
           let rec splitLines = (start, acc) => {
             let len = String.length(block);
             if (start >= len) {
               acc
             } else {
               let idx = ref(start);
               while (idx^ < len && block.[idx^] != '\n') {
                 incr(idx);
               };
               let line = String.sub(block, start, idx^ - start);
               let newAcc = Js.Array.concat(~other=[|line|], acc);
               if (idx^ < len && block.[idx^] == '\n') {
                 splitLines(idx^ + 1, newAcc);
               } else {
                 newAcc;
               }
             };
           };
           splitLines(0, [||])
         };

       let idRef = ref(None);
       let eventRef = ref(None);
       let dataLines = ref([||]);

       for (i in 0 to Array.length(lines) - 1) {
         let line = lines[i];
         if (String.length(line) > 0 && line.[0] == ':') {
           ()
         } else {
           let colonPos = ref(-1);
           for (j in 0 to String.length(line) - 1) {
             if (colonPos^ == -1 && line.[j] == ':') {
               colonPos := j;
             };
           };

           if (colonPos^ == -1) {
             let field = line;
             if (field == "data") {
               ()
             } else if (field == "event") {
               ()
             } else if (field == "id") {
               ()
             } else {
               ()
             };
           } else {
             let field = String.sub(line, 0, colonPos^);
             let valueStart = colonPos^ + 1;
             let valueStart =
               if (valueStart < String.length(line) && line.[valueStart] == ' ') {
                 valueStart + 1;
               } else {
                 valueStart;
               };
             let value = String.sub(line, valueStart, String.length(line) - valueStart);

             if (field == "data") {
               dataLines := Js.Array.concat(~other=[|value|], dataLines^);
             } else if (field == "event") {
               eventRef := Some(value);
             } else if (field == "id") {
               idRef := Some(value);
             } else {
               ()
             };
           };
         };
       };

       let rec joinData = (i, acc) => {
         let arr = dataLines^;
         if (i >= Array.length(arr)) {
           acc
         } else if (i == 0) {
           joinData(i + 1, arr[i]);
         } else {
           joinData(i + 1, acc ++ "\n" ++ arr[i]);
         };
       };

       {
         id: idRef^,
         event: eventRef^,
         data: joinData(0, ""),
       };
     }))
  |> Js.Array.filter(~f=(evt => String.length(evt.data) > 0));
};
