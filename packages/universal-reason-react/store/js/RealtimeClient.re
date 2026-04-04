[@platform js]
let mutationFrameString: (string, string) => string =
  [%raw
    {|
  function(actionId, actionString) {
    return JSON.stringify({type: "mutation", actionId: actionId, action: JSON.parse(actionString)});
  }
  |}];

[@platform native]
let mutationFrameString = (_actionId, _actionString) => "";

[@platform js]
let selectFrameString: (string, float) => string =
  [%raw
    {|
  function(subscription, updatedAt) {
    return JSON.stringify({type: "select", subscription: subscription, updatedAt: updatedAt});
  }
  |}];

[@platform native]
let selectFrameString = (_subscription, _updatedAt) => "";

[@platform js]
let pingFrameString: unit => string =
  [%raw
    {|
  function() {
    return JSON.stringify({type: "ping"});
  }
  |}];

[@platform native]
let pingFrameString = () => "";

[@platform native]
module Socket = {
  let defaultBaseUrl = "http://localhost:8899";
  let sendAction = (~actionId: string, ~action: StoreJson.json) => ();

  let subscribeSynced =
      (~subscription: string, ~updatedAt: float, ~onOpen=() => (), ~onPatch, ~onSnapshot, ~onAck, ~eventUrl: string, ~baseUrl: string, ()) => {
    let _ = subscription;
    let _ = updatedAt;
    let _ = onOpen;
    let _ = onPatch;
    let _ = onSnapshot;
    let _ = onAck;
    let _ = eventUrl;
    let _ = baseUrl;
    ();
  };
};

[@platform js]
module Socket = {
  let defaultBaseUrl = "http://localhost:8899";

  type websocket_state = {
    last_ping: float,
    last_pong: float,
  };

  external setInterval: (unit => unit, int) => int = "setInterval";
  external setTimeout: (unit => unit, int) => int = "setTimeout";

  let pingTimeoutSeconds = 5.0;
  let intervalRef: ref(option(int)) = ref(None);
  let sendRef: ref(option(string => unit)) = ref(None);

  let sendFrame = message =>
    switch (sendRef.contents) {
    | Some(send) => send(message)
    | None => ()
    };

  let sendAction = (~actionId: string, ~action: StoreJson.json) =>
    sendFrame(mutationFrameString(actionId, StoreJson.stringify(json => json, action)));

  let rec subscribeSynced =
          (~subscription: string, ~updatedAt: float, ~onOpen=() => (), ~onPatch, ~onSnapshot, ~onAck, ~eventUrl: string, ~baseUrl: string, ()) => {
    let url = Webapi.Url.makeWith(eventUrl, ~base=baseUrl);
    url->Webapi.Url.setProtocol("ws");

    let ws = WebSocket.make(url->Webapi.Url.href);
    sendRef := Some(message => {
      if (ws->WebSocket.readyState == 1) {
        ws->WebSocket.send_string(message);
      };
    });

    let timeoutSignal = Tilia.Core.signal;
    let lift = Tilia.Core.lift;
    let (lastPongTs, setLastPongTs) = timeoutSignal(0.0);
    let (lastPingTs, setLastPingTs) = timeoutSignal(0.0);
    let state =
      Tilia.Core.make({
        last_ping: lastPingTs->lift,
        last_pong: lastPongTs->lift,
      });

    let sendPing = () => {
      if (ws->WebSocket.readyState == 1) {
        sendFrame(pingFrameString());
        setLastPingTs(Js.Date.now());
      };
    };

    Tilia.Core.observe(() => {
      if (state.last_pong -. state.last_ping > pingTimeoutSeconds) {
        ws->WebSocket.close;
      };
    });

    switch (intervalRef.contents) {
    | Some(_) => ()
    | None => intervalRef := Some(setInterval(sendPing, Float.to_int(pingTimeoutSeconds *. 1000.0)))
    };

    WebSocket.onOpen(ws, () => {
      sendFrame(selectFrameString(subscription, updatedAt));
      onOpen();
    });

    WebSocket.onClose(ws, () => {
      sendRef := None;
      ignore(setTimeout(() => subscribeSynced(~subscription, ~updatedAt, ~onOpen, ~onPatch, ~onSnapshot, ~onAck, ~eventUrl, ~baseUrl, ()), 250));
    });

    WebSocket.onMessage(
      ws,
      event => {
        let data: string = event##data;
        switch (StoreJson.tryParse(data)) {
        | Some(json) =>
          switch (StoreJson.field(json, "type")) {
          | Some(rawType) =>
            switch (StoreJson.tryDecode(Melange_json.Primitives.string_of_json, rawType)) {
            | Some("pong") => setLastPongTs(Js.Date.now())
            | Some("patch") =>
              let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
              let timestamp =
                switch (StoreJson.optionalField(~json, ~fieldName="timestamp", ~decode=Melange_json.Primitives.float_of_json)) {
                | Some(value) => value
                | None => Js.Date.now()
                };
              onPatch(~payload, ~timestamp)
            | Some("snapshot") =>
              let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
              onSnapshot(payload)
            | Some("ack") =>
              let actionId = StoreJson.requiredField(~json, ~fieldName="actionId", ~decode=Melange_json.Primitives.string_of_json);
              let status = StoreJson.requiredField(~json, ~fieldName="status", ~decode=Melange_json.Primitives.string_of_json);
              let error = StoreJson.optionalField(~json, ~fieldName="error", ~decode=Melange_json.Primitives.string_of_json);
              onAck(actionId, status, error)
            | _ => ()
            }
          | None => ()
          }
        | None => ()
        };
      },
    );
  };
};
