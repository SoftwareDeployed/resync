/* BroadcastChannel API bindings - not available in melange-webapi */

// On native, BroadcastChannel is just a stub type since it's a browser API
[@platform native]
type t = unit;

[@platform native]
let make = (_name: string) => ();

[@platform native]
let postMessage = (_t: t, _message: string) => ();

[@platform native]
let setOnmessage = (_t: t, _handler: Js.t({. data: string}) => unit) => ();

[@platform native]
let close = (_t: t) => ();

[@platform native]
let name = (_t: t) => "";

// On JS, use proper Melange externals
[@platform js]
type t;

[@platform js]
[@mel.new] external make: string => t = "BroadcastChannel";

[@platform js]
[@mel.send] external postMessage: (t, string) => unit = "postMessage";

[@platform js]
[@mel.set] external setOnmessage: (t, Js.t({. data: string}) => unit) => unit = "onmessage";

[@platform js]
[@mel.send] external close: t => unit = "close";

[@platform js]
[@mel.get] external name: t => string = "name";
