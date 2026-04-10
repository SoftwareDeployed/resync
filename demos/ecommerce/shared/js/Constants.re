let cart_url = "/cart";
let event_url = "/_events";
let item_url = "/item";

let base_url =
  switch%platform (Runtime.platform) {
  | Client =>
    Webapi.Dom.Window.location(Webapi.Dom.window)
    ->Webapi.Dom.Location.origin
  | Server =>
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
