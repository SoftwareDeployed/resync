[@platform js]
let mutationFrameString = (actionId: string, actionString: string) => {
  let actionJson = Js.Json.parseExn(actionString);
  let jsonObj =
    Js.Dict.fromArray([|
      ("type", Js.Json.string("mutation")),
      ("actionId", Js.Json.string(actionId)),
      ("action", actionJson),
    |]);
  Js.Json.stringify(Js.Json.object_(jsonObj));
};

[@platform native]
let mutationFrameString = (_actionId, _actionString) => "";

[@platform js]
let selectFrameString = (subscription: string, updatedAt: float) => {
  let jsonObj =
    Js.Dict.fromArray([|
      ("type", Js.Json.string("select")),
      ("subscription", Js.Json.string(subscription)),
      ("updatedAt", Js.Json.number(updatedAt)),
    |]);
  Js.Json.stringify(Js.Json.object_(jsonObj));
};

[@platform native]
let selectFrameString = (_subscription, _updatedAt) => "";

[@platform js]
let pingFrameString = () => {
  let jsonObj = Js.Dict.fromArray([|("type", Js.Json.string("ping"))|]);
  Js.Json.stringify(Js.Json.object_(jsonObj));
};

[@platform native]
let pingFrameString = () => "";

[@platform native]
module Socket = {
  type websocket;

  type websocket_state = {
    last_ping: float,
    last_pong: float,
  };

  type connection_handle = {
    websocketRef: ref(option(websocket)),
    send: string => bool,
    pingIntervalRef: ref(option(int)),
    reconnectTimeoutRef: ref(option(int)),
    connectionState: ref(websocket_state),
    disposedRef: ref(bool),
  };

  let makeHandle = () => {
    let websocketRef = ref(None);
    let pingIntervalRef = ref(None);
    let reconnectTimeoutRef = ref(None);
    let connectionState =
      ref({last_ping: 0.0, last_pong: 0.0});
    let disposedRef = ref(false);

    {
      websocketRef,
      send: _message => false,
      pingIntervalRef,
      reconnectTimeoutRef,
      connectionState,
      disposedRef,
    };
  };

  let disposeHandle = handle => {
    handle.disposedRef := true;
    handle.websocketRef := None;
    handle.pingIntervalRef := None;
    handle.reconnectTimeoutRef := None;
  };

  let sendAction = (~handle: connection_handle, ~actionId: string, ~action: StoreJson.json) => {
    let _ = actionId;
    let _ = action;
    let _ = handle;
    false;
  };

  let sendFrame = (~handle: connection_handle, ~frame: string) => {
    let _ = handle;
    let _ = frame;
    false;
  };

  let subscribeSynced =
      (~subscription: string,
       ~updatedAt: float,
       ~onOpen=() => (),
       ~onClose=() => (),
       ~onPatch,
       ~onCustom=?,
       ~onMedia=?,
       ~onError=?,
       ~onSnapshot,
       ~onAck,
       ~eventUrl: string,
       ~baseUrl: string,
       ()) => {
    let _ = subscription;
    let _ = updatedAt;
    let _ = onOpen;
    let _ = onClose;
    let _ = onPatch;
    let _ = onCustom;
    let _ = onMedia;
    let _ = onError;
    let _ = onSnapshot;
    let _ = onAck;
    let _ = eventUrl;
    let _ = baseUrl;
    makeHandle();
  };
};

[@platform js]
module Socket = {
  type websocket;

  external setInterval: (unit => unit, int) => int = "setInterval";
  external clearInterval: int => unit = "clearInterval";
  external setTimeout: (unit => unit, int) => int = "setTimeout";
  external clearTimeout: int => unit = "clearTimeout";

  let pingIntervalMs = 5000.0;
  let pingTimeoutMs = 15000.0;

  /* Subscription callbacks for a single channel */
  type subscription_callbacks = {
    onOpen: unit => unit,
    onClose: unit => unit,
    onPatch: (~payload: StoreJson.json, ~timestamp: float) => unit,
    onSnapshot: StoreJson.json => unit,
    onAck: (string, string, option(string)) => unit,
    onCustom: option(StoreJson.json => unit),
    onMedia: option(StoreJson.json => unit),
    onError: option(string => unit),
    updatedAt: float,
    subscription: string,
    channel: string,
    disposed: ref(bool),
  };

  /* Connection state for ping/pong tracking */
  type connection_state = {
    mutable last_ping: float,
    mutable last_pong: float,
  };

  /* Multiplexed connection state - shared across all subscriptions */
  type multiplexed_state = {
    mutable websocket: option(websocket),
    mutable pingIntervalId: option(int),
    mutable reconnectTimeoutId: option(int),
    subscriptions: Js.Dict.t(subscription_callbacks),
    mutable connectionState: connection_state,
    mutable eventUrl: string,
    mutable baseUrl: string,
    mutable isConnecting: bool,
    mutable isClosing: bool,
  };

  /* Global singleton state per URL combination */
  let globalState: ref(option(multiplexed_state)) = ref(None);

  let getOrCreateState = (~eventUrl: string, ~baseUrl: string) => {
    switch (globalState.contents) {
    | Some(state) =>
      /* Update URLs in case they changed */
      state.eventUrl = eventUrl;
      state.baseUrl = baseUrl;
      state
    | None =>
      let state = {
        websocket: None,
        pingIntervalId: None,
        reconnectTimeoutId: None,
        subscriptions: Js.Dict.empty(),
        connectionState: {
          last_ping: Js.Date.now(),
          last_pong: Js.Date.now(),
        },
        eventUrl,
        baseUrl,
        isConnecting: false,
        isClosing: false,
      };
      globalState := Some(state);
      state
    };
  };

  let clearPingInterval = state =>
    switch (state.pingIntervalId) {
    | Some(intervalId) =>
      clearInterval(intervalId);
      state.pingIntervalId = None;
    | None => ()
    };

  let clearReconnectTimeout = state =>
    switch (state.reconnectTimeoutId) {
    | Some(timeoutId) => {
        clearTimeout(timeoutId);
        state.reconnectTimeoutId = None;
      }
    | None => ()
    };

  let sendFrame = (state, frame: string) =>
    switch (state.websocket) {
    | Some(ws) =>
      if (ws->WebSocket.readyState == 1) {
        ws->WebSocket.send_string(frame);
        true;
      } else {
        false;
      }
    | None => false
    };

  let updateLastPong = state => {
    state.connectionState = {
      ...state.connectionState,
      last_pong: Js.Date.now(),
    };
  };

  let updateLastPing = state => {
    state.connectionState = {
      ...state.connectionState,
      last_ping: Js.Date.now(),
    };
  };

  /* Route message to the correct subscription based on channel */
  let routeMessage = (state, json: StoreJson.json) => {
    Js.Console.log("[RealtimeClient] routeMessage called");
    switch (StoreJson.field(json, "channel")) {
    | Some(rawChannel) =>
      switch (StoreJson.tryDecode(Melange_json.Primitives.string_of_json, rawChannel)) {
      | Some(channel) =>
        Js.Console.log2("[RealtimeClient] Routing message for channel:", channel);
        switch (Js.Dict.get(state.subscriptions, channel)) {
        | Some(callbacks) =>
          Js.Console.log("[RealtimeClient] Found callbacks for channel");
          if (!callbacks.disposed.contents) {
            Js.Console.log("[RealtimeClient] Callbacks not disposed, processing message");
            switch (StoreJson.field(json, "type")) {
            | Some(rawType) =>
              switch (StoreJson.tryDecode(Melange_json.Primitives.string_of_json, rawType)) {
              | Some("pong") => updateLastPong(state)
              | Some("patch") =>
                let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
                let timestamp =
                  switch (StoreJson.optionalField(~json, ~fieldName="timestamp", ~decode=Melange_json.Primitives.float_of_json)) {
                  | Some(value) => value
                  | None => Js.Date.now()
                  };
                callbacks.onPatch(~payload, ~timestamp);
              | Some("snapshot") =>
                let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
                callbacks.onSnapshot(payload);
              | Some("ack") =>
                Js.Console.log("RealtimeClient: Received ack, invoking callback");
                let actionId = StoreJson.requiredField(~json, ~fieldName="actionId", ~decode=Melange_json.Primitives.string_of_json);
                let status = StoreJson.requiredField(~json, ~fieldName="status", ~decode=Melange_json.Primitives.string_of_json);
                let error = StoreJson.optionalField(~json, ~fieldName="error", ~decode=Melange_json.Primitives.string_of_json);
                Js.Console.log3("[RealtimeClient] Invoking onAck with:", actionId, status);
                callbacks.onAck(actionId, status, error);
              | Some("custom") =>
                let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
                switch (callbacks.onCustom) {
                | Some(handler) => handler(payload)
                | None => ()
                }
              | Some("media") =>
                let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
                switch (callbacks.onMedia) {
                | Some(handler) => handler(payload)
                | None => ()
                }
              | Some("error") =>
                let message = StoreJson.requiredField(~json, ~fieldName="message", ~decode=Melange_json.Primitives.string_of_json);
                switch (callbacks.onError) {
                | Some(handler) => handler(message)
                | None => ()
                }
              | _ => ()
              }
            | None => ()
            }
          }
        | None => ()
        }
      | None => ()
      }
    | None => ()
    };
  };

  /* Close websocket if no active subscriptions */
  let maybeClose = state => {
    let hasActive = ref(false);
    let subs = Js.Dict.entries(state.subscriptions);
    for (i in 0 to Array.length(subs) - 1) {
      let (_, callbacks) = subs[i];
      if (!callbacks.disposed.contents) {
        hasActive := true;
      };
    };
    if (!hasActive.contents) {
      state.isClosing = true;
      clearPingInterval(state);
      clearReconnectTimeout(state);
      switch (state.websocket) {
      | Some(ws) => {
          state.websocket = None;
          ws->WebSocket.close;
        }
      | None => ()
      };
      globalState := None;
    };
  };

  let rec connect = state => {
    if (!state.isConnecting && !state.isClosing) {
      switch (state.websocket) {
      | Some(_) => () /* Already connected */
      | None =>
        state.isConnecting = true;
        let url =
          if (state.baseUrl == "") {
            let loc = Webapi.Dom.window->Webapi.Dom.Window.location;
            let protocol = loc->Webapi.Dom.Location.protocol;
            let host = loc->Webapi.Dom.Location.host;
            let wsProtocol = protocol == "https:" ? "wss:" : "ws:";
            Webapi.Url.make(wsProtocol ++ "//" ++ host ++ state.eventUrl);
          } else {
            Webapi.Url.makeWith(state.eventUrl, ~base=state.baseUrl);
          };
        let isSecure = Webapi.Url.protocol(url) == "https:";
        url->Webapi.Url.setProtocol(isSecure ? "wss" : "ws");

        let ws: websocket = Obj.magic(WebSocket.make(url->Webapi.Url.href));
        state.websocket = Some(ws);
        state.connectionState = {
          last_ping: Js.Date.now(),
          last_pong: Js.Date.now(),
        };

        let sendPing = () => {
          if (ws->WebSocket.readyState == 1) {
            let _ = sendFrame(state, pingFrameString());
            updateLastPing(state);
          };
          let connState = state.connectionState;
          if (connState.last_ping -. connState.last_pong > pingTimeoutMs) {
            Js.Console.warn("RealtimeClient: Ping timeout, closing connection");
            ws->WebSocket.close;
          };
        };

        WebSocket.onOpen(ws, () => {
          state.isConnecting = false;
          clearPingInterval(state);
          state.pingIntervalId = Some(setInterval(sendPing, Float.to_int(pingIntervalMs)));
          /* Send select for all active subscriptions */
          let subs = Js.Dict.entries(state.subscriptions);
          for (i in 0 to Array.length(subs) - 1) {
            let (_, callbacks) = subs[i];
            if (!callbacks.disposed.contents) {
              let _ = sendFrame(state, selectFrameString(callbacks.subscription, callbacks.updatedAt));
              callbacks.onOpen();
            };
          };
        });

        WebSocket.onClose(ws, () => {
          state.websocket = None;
          state.isConnecting = false;
          clearPingInterval(state);
          if (!state.isClosing) {
            /* Notify all subscriptions of close */
            let subs = Js.Dict.entries(state.subscriptions);
            for (i in 0 to Array.length(subs) - 1) {
              let (_, callbacks) = subs[i];
              if (!callbacks.disposed.contents) {
                callbacks.onClose();
              };
            };
            /* Reconnect after delay */
            clearReconnectTimeout(state);
            state.reconnectTimeoutId = Some(
              setTimeout(
                () => {
                  state.reconnectTimeoutId = None;
                  connect(state);
                },
                250,
              ),
            );
          };
        });

        WebSocket.onMessage(ws, event => {
          let data: string = event##data;
          Js.Console.log2("[RealtimeClient] WebSocket raw message received:", data);
          updateLastPong(state);
          switch (StoreJson.tryParse(data)) {
          | Some(json) => routeMessage(state, json)
          | None => ()
          };
        });
      };
    };
  };

  /* Per-subscription connection handle */
  type connection_handle = {
    channel: string,
    send: string => bool,
    dispose: unit => unit,
    disposedRef: ref(bool),
  };

  let subscribeSynced =
      (~subscription: string,
       ~updatedAt: float,
       ~onOpen=() => (),
       ~onClose=() => (),
       ~onPatch,
       ~onCustom=?,
       ~onMedia=?,
       ~onError=?,
       ~onSnapshot,
       ~onAck,
       ~eventUrl: string,
       ~baseUrl: string,
       ()) => {
    Js.Console.log2("[RealtimeClient] subscribeSynced called for channel:", subscription);
    let state = getOrCreateState(~eventUrl, ~baseUrl);
    let disposedRef = ref(false);

    /* Extract channel ID from subscription (format: "type:id", e.g., "list:uuid") */
    let channelId =
      switch (Js.String.split(~sep=":", subscription)) {
      | [|_, id|] => id
      | _ => subscription
      };

    let callbacks = {
      onOpen,
      onClose,
      onPatch,
      onSnapshot,
      onAck,
      onCustom,
      onMedia,
      onError,
      updatedAt,
      subscription,
      channel: channelId,
      disposed: disposedRef,
    };

    /* Register subscription with channel ID as key */
    Js.Dict.set(state.subscriptions, channelId, callbacks);

    /* Ensure connection is established */
    connect(state);

    /* Send select immediately if already connected */
    switch (state.websocket) {
    | Some(ws) =>
      if (ws->WebSocket.readyState == 1) {
        let _ = sendFrame(state, selectFrameString(subscription, updatedAt));
        onOpen();
      }
    | None => ()
    };

    Js.Console.log2("[RealtimeClient] Subscription registered with channelId:", channelId);

    {
      channel: subscription,
      send: frame => sendFrame(state, frame),
      dispose: () => {
        if (!disposedRef.contents) {
          disposedRef := true;
          maybeClose(state);
        };
      },
      disposedRef,
    };
  };

  let sendAction = (~handle: connection_handle, ~actionId: string, ~action: StoreJson.json) =>
    handle.send(mutationFrameString(actionId, StoreJson.stringify(json => json, action)));

  let sendFrame = (~handle: connection_handle, ~frame: string) =>
    handle.send(frame);

  let disposeHandle = handle => {
    handle.dispose();
  };
};

[@platform native]
module Multiplexed = {
  type t;
  type subscription_handle = { channel: string, id: int };
  let make = (~eventUrl as _, ~baseUrl as _) => Obj.magic();
  let subscribe = (~channel as _, ~updatedAt as _, ~onOpen as _, ~onClose as _, ~onPatch as _, ~onSnapshot as _, ~onAck as _, _t) => { channel: "", id: 0 };
  let unsubscribe = (_t, _handle) => ();
  let sendAction = (~actionId as _, ~action as _, _t) => false;
  let dispose = (_t) => ();
};
