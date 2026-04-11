type fetch_event =
  | Chunk(string)
  | Error(string)
  | Done;

[@platform js]
let setupFetch: (string, string, string => unit, string => unit, unit => unit) => unit = [%raw {|
  function(url, body, onNext, onError, onDone) {
    fetch(url, {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: body
    }).then(response => {
      if (!response.ok) {
        onError("HTTP " + response.status);
        return;
      }
      const reader = response.body.getReader();
      const decoder = new TextDecoder("utf-8");
      function pump() {
        return reader.read().then(({ done, value }) => {
          if (done) {
            onDone();
            return;
          }
          onNext(decoder.decode(value));
          return pump();
        });
      }
      return pump();
    }).catch(err => {
      onError(String(err));
    });
  }
|}];

[@platform js]
let post = (~url: string, ~body: string) : StreamPipe.t(fetch_event) => {
  StreamPipe.make(~subscribe=(next => {
    setupFetch(
      url,
      body,
      data => next(Chunk(data)),
      msg => next(Error(msg)),
      () => next(Done)
    );
    () => ();
  }));
};

[@platform native]
let post = (~url: string, ~body: string) : StreamPipe.t(fetch_event) => {
  StreamPipe.make(~subscribe=(_next => () => ()));
};
