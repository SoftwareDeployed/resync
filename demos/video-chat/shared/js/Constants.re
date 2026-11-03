let event_url = "/_events";

let base_url =
  switch%platform () {
  | Melange =>
    Webapi.Dom.Window.location(Webapi.Dom.window)
    ->Webapi.Dom.Location.origin
  | Native =>
    switch (Sys.getenv_opt("VIDEO_CHAT_BASE_URL")) {
    | Some(url) => url
    | None => "http://localhost:8897"
    }
  };