type fetch_event =
  | Chunk(string)
  | Error(string)
  | Done;

[@platform js]
let setupFetch:
    (string, string, string => unit, string => unit, unit => unit) => unit => unit =
  [%raw {|
  function(url, body, onNext, onError, onDone) {
    let cancelled = false;
    let reader = null;
    const controller = new AbortController();

    fetch(url, {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: body,
      signal: controller.signal,
    }).then(response => {
      if (cancelled) {
        return;
      }
      if (!response.ok) {
        onError("HTTP " + response.status);
        return;
      }
      if (!response.body) {
        onError("Missing response body");
        return;
      }

      reader = response.body.getReader();
      const decoder = new TextDecoder("utf-8");

      function pump() {
        if (cancelled || !reader) {
          return;
        }
        return reader.read().then(({ done, value }) => {
          if (cancelled) {
            return;
          }
          if (done) {
            onDone();
            return;
          }
          onNext(decoder.decode(value, { stream: true }));
          return pump();
        }).catch(err => {
          if (!cancelled) {
            onError(String(err));
          }
        });
      }

      return pump();
    }).catch(err => {
      if (!cancelled && !(err && err.name === 'AbortError')) {
        onError(String(err));
      }
    });

    return function() {
      cancelled = true;
      try {
        controller.abort();
      } catch (_) {}
      if (reader) {
        try {
          reader.cancel().catch(function() {});
        } catch (_) {}
      }
    };
  }
|}];

[@platform js]
let post = (~url: string, ~body: string) : StreamPipe.t(fetch_event) => {
  StreamPipe.make(~subscribe=(next => {
    let cleanup = setupFetch(
      url,
      body,
      data => next(Chunk(data)),
      msg => next(Error(msg)),
      () => next(Done)
    );
    cleanup;
  }));
};

[@platform native]
let post = (~url: string, ~body: string) : StreamPipe.t(fetch_event) => {
  StreamPipe.make(~subscribe=(_next => () => ()));
};
