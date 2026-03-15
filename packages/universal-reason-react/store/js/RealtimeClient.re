module Socket = {
  [@platform native]
  let defaultBaseUrl = "http://localhost:8899";

  [@platform native]
  let subscribe = (
    ~set: 'config => unit,
    ~get: unit => 'config,
    ~premiseId: string,
    ~updatedAt: float,
    ~parsePatch: Js.Json.t => option('patch),
    ~applyPatch: ('config, 'patch) => 'config,
    ~decodeSnapshot: string => 'config,
    ~updatedAtOf: 'config => float,
    ~eventUrl: string,
    ~baseUrl: string,
  ) => {
    let _ = set;
    let _ = get;
    let _ = premiseId;
    let _ = updatedAt;
    let _ = parsePatch;
    let _ = applyPatch;
    let _ = decodeSnapshot;
    let _ = updatedAtOf;
    let _ = eventUrl;
    let _ = baseUrl;
    ();
  };

  [@platform js]
  let defaultBaseUrl = "http://localhost:8899";
  [@platform js]
  external globalThis: Js.Dict.t(int) = "globalThis";
  [@platform js]
  external setInterval: (. unit => unit, int) => int = "setInterval";

  [@platform js]
  type websocket_state = {
    last_ping: float,
    last_pong: float,
    updated_at: float,
  };

  [@platform js]
  let rec subscribe = (
    ~set: 'config => unit,
    ~get: unit => 'config,
    ~premiseId: string,
    ~updatedAt: float,
    ~parsePatch: Js.Json.t => option('patch),
    ~applyPatch: ('config, 'patch) => 'config,
    ~decodeSnapshot: string => 'config,
    ~updatedAtOf: 'config => float,
    ~eventUrl: string,
    ~baseUrl: string,
  ) => {
    let url =
      Webapi.Url.makeWith(
        eventUrl
        ++ "?premise_id="
        ++ premiseId
        ++ "&ts="
        ++ updatedAt->Float.to_string,
        ~base=baseUrl,
      );
    url->Webapi.Url.setProtocol("ws");

    let ws = WebSocket.make(url->Webapi.Url.href);
    let timeout = 5.0;
    let signal = Tilia.Core.signal;
    let lift = Tilia.Core.lift;
    let (lastPongTs, setLastPongTs) = signal(0.0);
    let (lastPingTs, setLastPingTs) = signal(0.0);
    let (updatedTs, setUpdatedTs) = signal(updatedAt);
    let state: websocket_state =
      Tilia.Core.make({
        last_ping: lastPingTs->lift,
        last_pong: lastPongTs->lift,
        updated_at: updatedTs->lift,
      });

    let sendPing = () =>
      if (ws->WebSocket.readyState == 1) {
        ws->WebSocket.send_string("ping");
        setLastPingTs(Js.Date.fromString("now")->Js.Date.getTime);
      };
    let select = () => ws->WebSocket.send_string("select " ++ premiseId);

    Tilia.Core.observe(() => {
      let elapsed = state.last_pong -. state.last_ping;
      if (elapsed > timeout) {
        ws->WebSocket.close;
        subscribe(
          ~set,
          ~get,
          ~premiseId,
          ~updatedAt=Js.Date.now(),
          ~parsePatch,
          ~applyPatch,
          ~decodeSnapshot,
          ~updatedAtOf,
          ~eventUrl,
          ~baseUrl,
        );
      };
    });

    switch (globalThis->Js.Dict.get("interval")) {
    | Some(_) => ()
    | None =>
      setInterval(. () => sendPing(), Float.to_int(timeout) * 1000)
      |> globalThis->Js.Dict.set("interval")
    };

    WebSocket.onOpen(ws, () => select());
    WebSocket.onClose(
      ws,
      () => {
        subscribe(
          ~set,
          ~get,
          ~premiseId,
          ~updatedAt=state.updated_at,
          ~parsePatch,
          ~applyPatch,
          ~decodeSnapshot,
          ~updatedAtOf,
          ~eventUrl,
          ~baseUrl,
        );
      },
    );
    WebSocket.onMessage(
      ws,
      event => {
        let data: string = event##data;
        if (data === "pong") {
          setLastPongTs(Js.Date.fromString("now")->Js.Date.getTime);
        } else {
          let json = Js.Json.parseExn(data);
          switch (parsePatch(json)) {
          | Some(patch) =>
            let current = get();
            let next = applyPatch(current, patch);
            setUpdatedTs(updatedAtOf(next));
            set(next);
          | None =>
            let snapshot = decodeSnapshot(data);
            setUpdatedTs(updatedAtOf(snapshot));
            set(snapshot);
          };
        };
      },
    );
  };
};
