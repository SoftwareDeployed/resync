let event_url = "/_events";

let base_url =
  switch%platform (Runtime.platform) {
  | Client =>
    [%raw {|
      (window.location.protocol + "//" + window.location.host)
    |}]
  | Server =>
    switch (Sys.getenv_opt("VIDEO_CHAT_BASE_URL")) {
    | Some(url) => url
    | None => "http://localhost:8897"
    }
  };