let event_url = "/_events";

let base_url =
  switch%platform (Runtime.platform) {
  | Client =>
    Webapi.Dom.Window.location(Webapi.Dom.window)
    ->Webapi.Dom.Location.origin
  | Server =>
    switch (Sys.getenv_opt("LLM_CHAT_BASE_URL")) {
    | Some(url) => url
    | None => "http://localhost:8897"
    }
  };
