[@platform native]
module Socket = {
  let defaultBaseUrl = "http://localhost:8899";
};

[@platform js]
module Socket = {
  let defaultBaseUrl = "http://localhost:8899";
  external globalThis: Js.Dict.t(int) = "globalThis";
  external setInterval: (. unit => unit, int) => int = "setInterval";

  type websocket_state = {
    last_ping: float,
    last_pong: float,
    updated_at: float,
  };

  let rec subscribe =
          (
            ~set: 'config => unit,
            ~get: unit => 'config,
            ~subscription: string,
            ~updatedAt: float,
            ~parsePatch: StoreJson.json => option('patch),
            ~applyPatch: ('config, 'patch) => 'config,
            ~decodeSnapshot: StoreJson.json => 'config,
            ~updatedAtOf: 'config => float,
            ~eventUrl: string,
            ~baseUrl: string,
          ) => {
    let url =
      Webapi.Url.makeWith(
        eventUrl
        ++ "?subscription="
        ++ subscription
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
    let select = () => ws->WebSocket.send_string("select " ++ subscription);

    Tilia.Core.observe(() => {
      let elapsed = state.last_pong -. state.last_ping;
      if (elapsed > timeout) {
        ws->WebSocket.close;
        subscribe(
          ~set,
          ~get,
          ~subscription,
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
    WebSocket.onClose(ws, () => {
      subscribe(
        ~set,
        ~get,
        ~subscription,
        ~updatedAt=state.updated_at,
        ~parsePatch,
        ~applyPatch,
        ~decodeSnapshot,
        ~updatedAtOf,
        ~eventUrl,
        ~baseUrl,
      )
    });
    WebSocket.onMessage(
      ws,
      event => {
        let data: string = event##data;
        if (data === "pong") {
          setLastPongTs(Js.Date.fromString("now")->Js.Date.getTime);
        } else {
          let parsedJson = StoreJson.tryParse(data);
          switch (parsedJson) {
          | Some(json) =>
            switch (parsePatch(json)) {
            | Some(patch) =>
              let current = get();
              let next = applyPatch(current, patch);
              setUpdatedTs(updatedAtOf(next));
              set(next);
            | None =>
              let snapshot = decodeSnapshot(json);
              setUpdatedTs(updatedAtOf(snapshot));
              set(snapshot);
            }
          | None => ()
          };
        };
      },
    );
  };
};
