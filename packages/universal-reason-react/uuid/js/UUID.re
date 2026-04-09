[@platform js]
[@mel.module "uuidv7"]
external uuidv7: unit => string = "uuidv7";

[@platform js]
let makeJs = () => uuidv7();

[@platform js]
let timestampJs = (uuid: string) => {
  // UUIDv7 format: 018ff67f-07cc-7e8b-8f5e-1f8c3e5b7a9b
  // First 48 bits (12 hex chars) are the Unix timestamp in milliseconds
  // Characters 0-7 (before first dash) + characters 9-12 (after first dash)
  let hexPart1 = String.sub(uuid, 0, 8);   // First 8 hex chars
  let hexPart2 = String.sub(uuid, 9, 4);   // Next 4 hex chars (after first dash)
  let hexTimestamp = "0x" ++ hexPart1 ++ hexPart2;
  
  switch (int_of_string_opt(hexTimestamp)) {
  | Some(ts) => float_of_int(ts)
  | None => 0.0
  };
};

[@platform native]
let makeJs = () => {
  let now = Unix.gettimeofday();
  let time_ms = Int64.of_float(now *. 1000.0);
  let rand_state = Random.State.make_self_init();
  Uuidm.v7_non_monotonic_gen(
    ~now_ms=() => time_ms,
    rand_state,
    (),
  )->Uuidm.to_string;
};

[@platform native]
let timestampJs = (uuid: string) => {
  switch (Uuidm.of_string(uuid)) {
  | Some(u) =>
    switch (Uuidm.time_ms(u)) {
    | Some(ms) => Int64.to_float(ms)
    | None => 0.0
    }
  | None => 0.0
  }
};

let make = () => makeJs();

let timestamp = uuid => timestampJs(uuid);
