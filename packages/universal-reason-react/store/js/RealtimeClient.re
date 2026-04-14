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

  /* Send a raw string frame. Returns true if sent, false if websocket not open. */
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

  type websocket_state = {
    last_ping: float,
    last_pong: float,
  };

  /* Per-subscription connection handle with explicit send/reconnect/dispose
     semantics. Store-scoped ownership: the synced runtime creates, owns, and
     disposes handles; no singleton active subscription state remains at the
     module level. */
  type connection_handle = {
    websocketRef: ref(option(websocket)),
    send: string => bool,
    pingIntervalRef: ref(option(int)),
    reconnectTimeoutRef: ref(option(int)),
    connectionState: ref(websocket_state),
    disposedRef: ref(bool),
  };

  external setInterval: (unit => unit, int) => int = "setInterval";
  external clearInterval: int => unit = "clearInterval";
  external setTimeout: (unit => unit, int) => int = "setTimeout";
  external clearTimeout: int => unit = "clearTimeout";

  let pingIntervalMs = 5000.0;
  let pingTimeoutMs = 15000.0;

  let makeHandle = () => {
    let websocketRef = ref(None);
    let pingIntervalRef = ref(None);
    let reconnectTimeoutRef = ref(None);
    let connectionState =
      ref({
        last_ping: Js.Date.now(),
        last_pong: Js.Date.now(),
      });
    let disposedRef = ref(false);

    {
      websocketRef,
      send: message =>
        switch (websocketRef.contents) {
        | Some(ws) =>
          if (ws->WebSocket.readyState == 1) {
            ws->WebSocket.send_string(message);
            true;
          } else {
            false;
          }
        | None => false
        },
      pingIntervalRef,
      reconnectTimeoutRef,
      connectionState,
      disposedRef,
    };
  };

  let clearPingInterval = handle =>
    switch (handle.pingIntervalRef.contents) {
    | Some(intervalId) =>
      clearInterval(intervalId);
      handle.pingIntervalRef := None;
    | None => ()
    };

  let clearReconnectTimeout = handle =>
    switch (handle.reconnectTimeoutRef.contents) {
    | Some(timeoutId) => {
        clearTimeout(timeoutId);
        handle.reconnectTimeoutRef := None;
      }
    | None => ()
    };

  /* Explicit dispose: idempotent; safe to call multiple times. Invalidates
     the handle so subsequent sends return false; closes websocket; clears
     timers. */
  let disposeHandle = handle => {
    handle.disposedRef := true;
    clearPingInterval(handle);
    clearReconnectTimeout(handle);
    switch (handle.websocketRef.contents) {
    | Some(ws) => {
        handle.websocketRef := None;
        ws->WebSocket.close;
      }
    | None => ()
    };
  };

  let sendAction = (~handle: connection_handle, ~actionId: string, ~action: StoreJson.json) =>
    handle.send(
      mutationFrameString(actionId, StoreJson.stringify(json => json, action)),
    );

  /* Send a raw string frame. Returns true if sent, false if websocket not open.
     Requires explicit handle - no singleton active connection state. */
  let sendFrame = (~handle: connection_handle, ~frame: string) =>
    handle.send(frame);

  let updateLastPing = handle => {
    let currentState = handle.connectionState.contents;
    handle.connectionState := {
      ...currentState,
      last_ping: Js.Date.now(),
    };
  };

  let updateLastPong = handle => {
    let currentState = handle.connectionState.contents;
    handle.connectionState := {
      ...currentState,
      last_pong: Js.Date.now(),
    };
  };

  let rec connect =
      (~handle: connection_handle,
       ~subscription: string,
       ~updatedAt: float,
       ~onOpen=() => (),
       ~onClose=() => (),
       ~onPatch,
       ~onCustom,
       ~onMedia,
       ~onError,
       ~onSnapshot,
       ~onAck,
       ~eventUrl: string,
       ~baseUrl: string,
       ()) => {
    if (!handle.disposedRef.contents) {
      let url = Webapi.Url.makeWith(eventUrl, ~base=baseUrl);
      let isSecure = Webapi.Url.protocol(url) == "https:";
      url->Webapi.Url.setProtocol(isSecure ? "wss" : "ws");

      let ws: websocket = Obj.magic(WebSocket.make(url->Webapi.Url.href));
      handle.websocketRef := Some(ws);
      handle.connectionState := {
        last_ping: Js.Date.now(),
        last_pong: Js.Date.now(),
      };

      let sendPing = () => {
        if (ws->WebSocket.readyState == 1) {
          let _ = handle.send(pingFrameString());
          updateLastPing(handle);
        };
        let state = handle.connectionState.contents;
        if (state.last_pong -. state.last_ping > pingTimeoutMs) {
          Js.Console.warn("RealtimeClient: Ping timeout, closing connection");
          ws->WebSocket.close;
        };
      };

      clearPingInterval(handle);
      handle.pingIntervalRef :=
        Some(setInterval(sendPing, Float.to_int(pingIntervalMs)));

      WebSocket.onOpen(ws, () => {
        let _ = handle.send(selectFrameString(subscription, updatedAt));
        onOpen();
      });

      WebSocket.onClose(ws, () => {
        handle.websocketRef := None;
        clearPingInterval(handle);
        if (!handle.disposedRef.contents) {
          onClose();
          clearReconnectTimeout(handle);
          handle.reconnectTimeoutRef :=
            Some(
              setTimeout(
                () => {
                  handle.reconnectTimeoutRef := None;
                  connect(
                    ~handle,
                    ~subscription,
                    ~updatedAt,
                    ~onOpen,
                    ~onClose,
                    ~onPatch,
                    ~onCustom,
                    ~onMedia,
                    ~onError,
                    ~onSnapshot,
                    ~onAck,
                    ~eventUrl,
                    ~baseUrl,
                    (),
                  );
                },
                250,
              ),
            );
        };
      });

      WebSocket.onMessage(
        ws,
        event => {
          let data: string = event##data;
          updateLastPong(handle);
          switch (StoreJson.tryParse(data)) {
          | Some(json) =>
            switch (StoreJson.field(json, "type")) {
            | Some(rawType) =>
              switch (StoreJson.tryDecode(Melange_json.Primitives.string_of_json, rawType)) {
              | Some("pong") => updateLastPong(handle)
              | Some("patch") =>
                let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
                let timestamp =
                  switch (StoreJson.optionalField(~json, ~fieldName="timestamp", ~decode=Melange_json.Primitives.float_of_json)) {
                  | Some(value) => value
                  | None => Js.Date.now()
                  };
                onPatch(~payload, ~timestamp)
              | Some("snapshot") =>
                Js.Console.log("RealtimeClient: Received snapshot");
                let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
                onSnapshot(payload)
              | Some("ack") =>
                Js.Console.log("RealtimeClient: Received ack");
                let actionId = StoreJson.requiredField(~json, ~fieldName="actionId", ~decode=Melange_json.Primitives.string_of_json);
                let status = StoreJson.requiredField(~json, ~fieldName="status", ~decode=Melange_json.Primitives.string_of_json);
                let error = StoreJson.optionalField(~json, ~fieldName="error", ~decode=Melange_json.Primitives.string_of_json);
                onAck(actionId, status, error)
              | Some("custom") =>
                let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
                switch (onCustom) {
                | Some(handler) => handler(payload)
                | None => ()
                }
              | Some("media") =>
                let payload = StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
                switch (onMedia) {
                | Some(handler) => handler(payload)
                | None => ()
                }
              | Some("error") =>
                let message = StoreJson.requiredField(~json, ~fieldName="message", ~decode=Melange_json.Primitives.string_of_json);
                switch (onError) {
                | Some(handler) => handler(message)
                | None => ()
                }
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

  /* Create a store-scoped connection handle and begin the subscription.
     Returns the handle for explicit lifecycle control (send/reconnect/dispose).
     Callbacks are captured at subscription time and passed directly to the
     connection loop; no singleton callback registry or active handle state
     remains at the module level. */
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
    let handle = makeHandle();
    connect(
      ~handle,
      ~subscription,
      ~updatedAt,
      ~onOpen,
      ~onClose,
      ~onPatch,
      ~onCustom,
      ~onMedia,
      ~onError,
      ~onSnapshot,
      ~onAck,
      ~eventUrl,
      ~baseUrl,
      (),
    );
    handle;
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
