type ws_protocols = option(array(string));
type ws_event = {. data: string };
type t;

[@mel.new]
external makeWithProtocols: (. string, ws_protocols) => t = "WebSocket";

[@mel.new] external make: string => t = "WebSocket";

[@mel.send] external close: t => unit = "close";

[@mel.send] external send_string: (t, string) => unit = "send";

[@mel.get] external readyState: t => int = "readyState";

// Type for event names
type event_name = string;

[@mel.send]
external addEventListener: (t, event_name, 'a) => unit = "addEventListener";

// Helper functions with explicit string arguments
let onOpen = (ws, callback) => addEventListener(ws, "open", callback);

let onClose = (ws, callback) => addEventListener(ws, "close", callback);

let onMessage = (ws, callback) => addEventListener(ws, "message", callback);
