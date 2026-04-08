type ws_protocols = option(array(string));
type ws_event = {. data: string };

[@mel.new]
external makeWithProtocols: (. string, ws_protocols) => 'a = "WebSocket";

[@mel.new] external make: string => 'a = "WebSocket";

[@mel.send] external close: 'a => unit = "close";

[@mel.send] external send_string: ('a, string) => unit = "send";

[@mel.get] external readyState: 'a => int = "readyState";

// Type for event names
type event_name = string;

[@mel.send]
external addEventListener: ('a, event_name, 'b) => unit = "addEventListener";

// Helper functions with explicit string arguments
let onOpen = (ws, callback) => addEventListener(ws, "open", callback);

let onClose = (ws, callback) => addEventListener(ws, "close", callback);

let onMessage = (ws, callback) => addEventListener(ws, "message", callback);
