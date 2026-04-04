[@platform js]
let makeJs: unit => string =
  [%raw
    {|
  function() {
    var timestamp = Date.now();
    var bytes = new Uint8Array(16);
    crypto.getRandomValues(bytes);
    bytes[0] = (timestamp / 0x10000000000) & 0xff;
    bytes[1] = (timestamp / 0x100000000) & 0xff;
    bytes[2] = (timestamp / 0x1000000) & 0xff;
    bytes[3] = (timestamp / 0x10000) & 0xff;
    bytes[4] = (timestamp / 0x100) & 0xff;
    bytes[5] = timestamp & 0xff;
    bytes[6] = (bytes[6] & 0x0f) | 0x70;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    var hex = function(byte) { return byte.toString(16).padStart(2, "0"); };
    return (
      hex(bytes[0]) + hex(bytes[1]) + hex(bytes[2]) + hex(bytes[3]) +
      "-" + hex(bytes[4]) + hex(bytes[5]) +
      "-" + hex(bytes[6]) + hex(bytes[7]) +
      "-" + hex(bytes[8]) + hex(bytes[9]) +
      "-" + hex(bytes[10]) + hex(bytes[11]) + hex(bytes[12]) + hex(bytes[13]) + hex(bytes[14]) + hex(bytes[15])
    );
  }
  |}];

[@platform native]
let makeJs = () => "00000000-0000-7000-8000-000000000000";

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
let timestampJs = (_uuid: string) => 0.0;

let make = () => makeJs();

let timestamp = uuid => timestampJs(uuid);
