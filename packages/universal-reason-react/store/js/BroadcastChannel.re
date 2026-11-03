/* BroadcastChannel API bindings - not available in melange-webapi */

// On native, BroadcastChannel is just a stub type since it's a browser API
[@native_only]
type t = unit;

[@native_only]
let make = (_name: string) => ();

[@native_only]
let postMessage = (_t: t, _message: string) => ();

[@native_only]
let setOnmessage = (_t: t, _handler: Js.t({. data: string}) => unit) => ();

[@native_only]
let close = (_t: t) => ();

[@native_only]
let name = (_t: t) => "";

// On JS, use proper Melange externals
[@melange_only]
type t;

[@melange_only]
[@mel.new] external make: string => t = "BroadcastChannel";

[@melange_only]
[@mel.send] external postMessage: (t, string) => unit = "postMessage";

[@melange_only]
[@mel.set] external setOnmessage: (t, Js.t({. data: string}) => unit) => unit = "onmessage";

[@melange_only]
[@mel.send] external close: t => unit = "close";

[@melange_only]
[@mel.get] external name: t => string = "name";
