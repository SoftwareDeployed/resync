let cart_url = "/cart";
let event_url = "/_events";
let item_url = "/item";

/* Origin of the live-support chat backend (the llm-chat demo server).
   The support widget opens its own multiplexed websocket there. */
[@melange_only]
let llm_chat_base_url = "http://127.0.0.1:8897";

[@native_only]
let llm_chat_base_url = "";

let base_url =
  switch%platform () {
  | Melange =>
    Webapi.Dom.Window.location(Webapi.Dom.window)
    ->Webapi.Dom.Location.origin
  | Native =>
    switch (Sys.getenv_opt("API_BASE_URL")) {
    | Some(url) => url
    | None => failwith("API_BASE_URL environment variable is required")
    }
  };
let premise_routes: list(string) = [
  item_url->String.sub(1, item_url->String.length - 1),
];
let system_routes = [
  cart_url->String.sub(1, cart_url->String.length - 1),
  event_url->String.sub(1, event_url->String.length - 1),
];
