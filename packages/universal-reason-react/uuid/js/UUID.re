[@platform js]
[@mel.module "uuidv7"]
external uuidv7: unit => string = "uuidv7";

[@platform js]
let makeJs = () => uuidv7();

[@platform js]
let timestampJs: string => float =
  [%raw
    {|
  function(uuid) {
    var hex = uuid.slice(0, 8) + uuid.slice(9, 13);
    return parseInt(hex, 16);
  }
  |}];

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
