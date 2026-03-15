type ws_protocols = option(array(string));
type ws_event = {. data: string};

[@platform native]
let makeWithProtocols = (. _url, _protocols) => Obj.magic(());
[@platform js]
[@mel.new]
external makeWithProtocols: (. string, ws_protocols) => 'a = "WebSocket";

[@platform native]
let make = _url => Obj.magic(());
[@platform js]
[@mel.new]
external make: string => 'a = "WebSocket";

[@platform native]
let close = _ => ();
[@platform js]
[@mel.send]
external close: unit => unit = "close";

[@platform native]
let send_string = (_, _) => ();
[@platform js]
[@mel.send]
external send_string: ('a, string) => unit = "send";

[@platform native]
let readyState = _ => 0;
[@platform js]
[@mel.get]
external readyState: 'a => int = "readyState";

// Type for event names
type event_name = string;

[@platform native]
let addEventListener = (_, _, _) => ();
[@platform js]
[@mel.send]
external addEventListener: ('a, event_name, 'b) => unit = "addEventListener";

// Helper functions with explicit string arguments
let onOpen = (ws, callback) => addEventListener(ws, "open", callback);

let onClose = (ws, callback) => addEventListener(ws, "close", callback);

let onMessage = (ws, callback) => addEventListener(ws, "message", callback);
