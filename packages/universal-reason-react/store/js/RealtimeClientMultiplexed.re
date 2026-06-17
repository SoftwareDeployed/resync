type pending_mutation = (string, StoreJson.json);

let hasPendingMutation = (~actionId: string, pending: array(pending_mutation)) =>
  pending->Js.Array.some(~f=((pendingActionId, _action)) =>
    pendingActionId == actionId
  );

let enqueuePendingMutation =
    (
      ~actionId: string,
      ~action: StoreJson.json,
      pending: array(pending_mutation),
    )
    : array(pending_mutation) =>
  if (hasPendingMutation(~actionId, pending)) {
    pending;
  } else {
    pending->Js.Array.concat(~other=[|(actionId, action)|]);
  };

let removePendingMutation =
    (~actionId: string, pending: array(pending_mutation))
    : array(pending_mutation) =>
  pending->Js.Array.filter(~f=((pendingActionId, _action)) =>
    pendingActionId != actionId
  );

[@platform js]
module Multiplexed = {
  type websocket = WebSocket.t;

  type callbacks = {
    id: int,
    subscription: string,
    updatedAt: float,
    onOpen: unit => unit,
    onClose: unit => unit,
    onPatch: (~payload: StoreJson.json, ~timestamp: float) => unit,
    onSnapshot: StoreJson.json => unit,
    onAck: (string, string, option(string)) => unit,
    onCustom: option(StoreJson.json => unit),
    onMedia: option(StoreJson.json => unit),
    onError: option(string => unit),
  };

  type t = {
    websocketRef: ref(option(websocket)),
    subscriptionsRef: ref(Js.Dict.t(array(callbacks))),
    nextIdRef: ref(int),
    eventUrl: string,
    baseUrl: string,
    disposedRef: ref(bool),
    idleCloseRef: ref(bool),
    pingIntervalRef: ref(option(int)),
    reconnectTimeoutRef: ref(option(int)),
    pendingMutationsRef: ref(array(pending_mutation)),
    lastPingRef: ref(float),
    lastPongRef: ref(float),
  };

  type subscription_handle = { channel: string, id: int };

  let pingIntervalMs = 5000;
  let pingTimeoutMs = 15000.0;

  external setInterval: (unit => unit, int) => int = "setInterval";
  external clearInterval: int => unit = "clearInterval";
  external setTimeout: (unit => unit, int) => int = "setTimeout";
  external clearTimeout: int => unit = "clearTimeout";
  [@mel.scope "window"] external window: Webapi.Dom.Window.t = "window";
  type response;
  [@mel.get] external responseOk: response => bool = "ok";
  external fetch: string => Js.Promise.t(response) = "fetch";

  type ready_probe = {mutable delay: int, mutable timer: option(int), mutable probing: bool};
  let readyProbeState: ref(Js.Dict.t(ready_probe)) = ref(Js.Dict.empty());

  let closeOnPageHide = (socket: websocket) => {
    let close = _event =>
      if (socket->WebSocket.readyState == 0 || socket->WebSocket.readyState == 1) {
        socket->WebSocket.close;
      };
    Webapi.Dom.Window.addEventListenerWithOptions(
      "pagehide",
      close,
      {"capture": false, "once": true, "passive": true},
      window,
    );
    Webapi.Dom.Window.addEventListenerWithOptions(
      "beforeunload",
      close,
      {"capture": false, "once": true, "passive": true},
      window,
    );
  };

  let readyUrlOfHref = href => {
    let url = Webapi.Url.make(href);
    Webapi.Url.setProtocol(url, Webapi.Url.protocol(url) == "wss:" ? "https:" : "http:");
    Webapi.Url.setPathname(url, "/readyz");
    Webapi.Url.setSearch(url, "");
    Webapi.Url.setHash(url, "");
    Webapi.Url.href(url);
  };

  let rec openWhenReady = (href, openSocket) => {
    let probes = readyProbeState.contents;
    let probe =
      switch (Js.Dict.get(probes, href)) {
      | Some(probe) => probe
      | None =>
        let probe = {delay: 250, timer: None, probing: false};
        Js.Dict.set(probes, href, probe);
        readyProbeState := probes;
        probe;
      };
    if (!probe.probing) {
      let retry = () => {
        switch (probe.timer) {
        | Some(timer) => clearTimeout(timer)
        | None => ()
        };
        let delay = probe.delay;
        probe.delay = min(max(probe.delay * 2, 500), 10000);
        probe.timer =
          Some(
            setTimeout(
              () => {
                probe.timer = None;
                openWhenReady(href, openSocket);
              },
              delay,
            ),
          );
      };
      probe.probing = true;
      fetch(readyUrlOfHref(href))
      |> Js.Promise.then_(response => {
           probe.probing = false;
           if (responseOk(response)) {
             probe.delay = 250;
             openSocket();
           } else {
             retry();
           };
           Js.Promise.resolve();
         })
      |> Js.Promise.catch(_error => {
           probe.probing = false;
           retry();
           Js.Promise.resolve();
         })
      |> ignore;
    };
  };

  let resetReadyBackoff = href =>
    switch (Js.Dict.get(readyProbeState.contents, href)) {
    | Some(probe) => probe.delay = 250
    | None => ()
    };

  let clearPingInterval = (t: t) => {
    switch (t.pingIntervalRef.contents) {
    | Some(intervalId) =>
      clearInterval(intervalId);
      t.pingIntervalRef := None;
    | None => ()
    };
  };

  let clearReconnectTimeout = (t: t) => {
    switch (t.reconnectTimeoutRef.contents) {
    | Some(timeoutId) =>
      clearTimeout(timeoutId);
      t.reconnectTimeoutRef := None;
    | None => ()
    };
  };

  let make = (~eventUrl: string, ~baseUrl: string) => {
    websocketRef: ref(None),
    subscriptionsRef: ref(Js.Dict.empty()),
    nextIdRef: ref(0),
    eventUrl,
    baseUrl,
    disposedRef: ref(false),
    idleCloseRef: ref(false),
    pingIntervalRef: ref(None),
    reconnectTimeoutRef: ref(None),
    pendingMutationsRef: ref([||]),
    lastPingRef: ref(Js.Date.now()),
    lastPongRef: ref(Js.Date.now()),
  };

  let isCurrentSocket = (t: t, ws: websocket) =>
    switch (t.websocketRef.contents) {
    | Some(currentWs) => currentWs == ws
    | None => false
    };

  let optionalStringField = (~json, ~fieldName) =>
    StoreJson.optionalField(
      ~json,
      ~fieldName,
      ~decode=Melange_json.Primitives.string_of_json,
    );

  let notifyAck = (t: t, json: StoreJson.json) =>
    switch (
      optionalStringField(~json, ~fieldName="actionId"),
      optionalStringField(~json, ~fieldName="status"),
    ) {
    | (Some(actionId), Some(status)) =>
      let error = optionalStringField(~json, ~fieldName="error");
      t.subscriptionsRef.contents
      ->Js.Dict.entries
      ->Js.Array.forEach(~f=((_, callbacks)) =>
          callbacks
          ->Js.Array.forEach(~f=callback =>
              callback.onAck(actionId, status, error)
            )
        );
    | _ => ()
    };

  let unsubscribeFrameString = (subscription: string) =>
    StoreJson.stringify(
      json => json,
      StoreJson.Object.make(dict => {
        StoreJson.Object.setString(dict, "type", "unsubscribe");
        StoreJson.Object.setString(dict, "channel", subscription);
      }),
    );

  let rec routeMessage = (t: t, json: StoreJson.json) => {
    let messageType =
      StoreJson.field(json, "type")
      |> Option.map(Melange_json.Primitives.string_of_json);
    switch (messageType) {
    | Some("batch") =>
      switch (StoreJson.field(json, "messages")) {
      | Some(rawMessages) =>
        switch (
          StoreJson.tryDecode(
            Melange_json.Primitives.array_of_json(message => message),
            rawMessages,
          )
        ) {
        | Some(messages) =>
          messages->Js.Array.forEach(~f=message => routeMessage(t, message))
        | None => ()
        }
      | None => ()
      }
    | Some("pong") => ()
    | Some("ack") => notifyAck(t, json)
    | _ =>
      switch (
        StoreJson.field(json, "channel")
        |> Option.map(Melange_json.Primitives.string_of_json)
      ) {
      | Some(channel) =>
        switch (Js.Dict.get(t.subscriptionsRef.contents, channel)) {
        | Some(callbacks) =>
          switch (messageType) {
          | Some("patch") =>
            let payload =
              switch (StoreJson.field(json, "payload")) {
              | Some(payload) => payload
              | None => json
              };
            let timestamp =
              switch (
                StoreJson.optionalField(
                  ~json,
                  ~fieldName="timestamp",
                  ~decode=Melange_json.Primitives.float_of_json,
                )
              ) {
              | Some(value) => value
              | None => Js.Date.now()
              };
            callbacks
            ->Js.Array.forEach(
                ~f=callback => callback.onPatch(~payload, ~timestamp),
              );
          | Some("snapshot") =>
            let payload =
              StoreJson.requiredField(
                ~json,
                ~fieldName="payload",
                ~decode=value => value,
              );
            callbacks->Js.Array.forEach(~f=callback => callback.onSnapshot(payload));
          | Some("ack") =>
            notifyAck(t, json)
          | Some("custom") =>
            let payload =
              StoreJson.requiredField(
                ~json,
                ~fieldName="payload",
                ~decode=value => value,
              );
            callbacks->Js.Array.forEach(~f=callback =>
                switch (callback.onCustom) {
                | Some(handler) => handler(payload)
                | None => ()
                }
              );
          | Some("media") =>
            let payload =
              StoreJson.requiredField(
                ~json,
                ~fieldName="payload",
                ~decode=value => value,
              );
            callbacks->Js.Array.forEach(~f=callback =>
                switch (callback.onMedia) {
                | Some(handler) => handler(payload)
                | None => ()
                }
              );
          | Some("error") =>
            let message =
              StoreJson.requiredField(
                ~json,
                ~fieldName="message",
                ~decode=Melange_json.Primitives.string_of_json,
              );
            callbacks->Js.Array.forEach(~f=callback =>
                switch (callback.onError) {
                | Some(handler) => handler(message)
                | None => ()
                }
              );
          | _ => ()
          }
        | None => ()
        }
      | None => ()
      }
    };
  };

  let rec connect = (t: t) => {
    if (!t.disposedRef.contents) {
      t.idleCloseRef := false;
      let url = Webapi.Url.makeWith(t.eventUrl, ~base=t.baseUrl);
      let isSecure = Webapi.Url.protocol(url) == "https:";
      url->Webapi.Url.setProtocol(isSecure ? "wss" : "ws");
      let href = url->Webapi.Url.href;

      let openSocket = () => {
        switch (t.websocketRef.contents) {
        | Some(_) => ()
        | None when t.disposedRef.contents => ()
        | None =>
        let ws: websocket = WebSocket.make(href);
        t.websocketRef := Some(ws);
        closeOnPageHide(ws);

      let sendPing = () => {
        switch (t.websocketRef.contents) {
        | Some(currentWs) when currentWs == ws && ws->WebSocket.readyState == 1 =>
          ws->WebSocket.send_string(RealtimeClient.pingFrameString());
          t.lastPingRef := Js.Date.now();
          if (t.lastPingRef.contents -. t.lastPongRef.contents > pingTimeoutMs) {
            Js.Console.warn("RealtimeClientMultiplexed: Ping timeout, closing connection");
            ws->WebSocket.close;
          };
        | _ => ()
        };
      };

      let handleOpen = () => {
        if (isCurrentSocket(t, ws)) {
          resetReadyBackoff(href);
          t.lastPingRef := Js.Date.now();
          t.lastPongRef := Js.Date.now();
          clearPingInterval(t);
          t.pingIntervalRef := Some(setInterval(sendPing, pingIntervalMs));
          /* Send select for all active subscriptions */
          t.subscriptionsRef.contents
          ->Js.Dict.entries
          ->Js.Array.forEach(~f=((_, callbacks)) => {
              if (Js.Array.length(callbacks) > 0) {
                callbacks
                ->Js.Array.map(~f=callback =>
                    (callback.subscription, callback.updatedAt)
                  )
                ->RealtimeClient.uniqueSelectRequests
                ->Js.Array.forEach(~f=((subscription, updatedAt)) => {
                    let _ =
                      t.websocketRef.contents
                      |> Option.map(ws => {
                          ws->WebSocket.send_string(
                            RealtimeClient.selectFrameString(
                              subscription,
                              updatedAt,
                            ),
                          )
                        });
                    ();
                  });
                callbacks->Js.Array.forEach(~f=callback => callback.onOpen());
              };
            });

          /* Send pending mutations */
          let pending = t.pendingMutationsRef.contents;
          t.pendingMutationsRef := [||];
          pending->Js.Array.forEach(~f=((actionId, action)) => {
            let _ = sendAction(~actionId, ~action, t);
          });
        };
      };

      WebSocket.onOpen(ws, handleOpen);

      WebSocket.onClose(ws, () => {
        let wasIdleClose = t.idleCloseRef.contents;
        t.idleCloseRef := false;
        let isCurrentSocket =
          switch (t.websocketRef.contents) {
          | Some(currentWs) => currentWs == ws
          | None => false
          };
        if (isCurrentSocket) {
          t.websocketRef := None;
          clearPingInterval(t);
        };
        if (isCurrentSocket && !t.disposedRef.contents && !wasIdleClose) {
          t.subscriptionsRef.contents
          ->Js.Dict.entries
          ->Js.Array.forEach(~f=((_, callbacks)) =>
              callbacks->Js.Array.forEach(~f=callback => callback.onClose())
            );
          t.reconnectTimeoutRef :=
            Some(
              setTimeout(
                () => {
                  t.reconnectTimeoutRef := None;
                  connect(t);
                },
                1000,
              ),
            );
        };
      });

      WebSocket.onMessage(ws, event => {
        if (isCurrentSocket(t, ws)) {
          let data: string = event##data;
          t.lastPongRef := Js.Date.now();
          switch (StoreJson.tryParse(data)) {
          | Some(json) => routeMessage(t, json)
          | None => ()
          };
        };
      });

      if (ws->WebSocket.readyState == 1) {
        let _ = setTimeout(handleOpen, 0);
        ();
      };
        };
      };
      openWhenReady(href, openSocket);
    };
  }

  and sendAction = (~actionId: string, ~action: StoreJson.json, t: t) => {
    switch (t.websocketRef.contents) {
    | Some(ws) when ws->WebSocket.readyState == 1 =>
      t.pendingMutationsRef :=
        removePendingMutation(~actionId, t.pendingMutationsRef.contents);
      ws->WebSocket.send_string(
        RealtimeClient.mutationFrameString(
          actionId,
          action,
        ),
      );
      true;
    | _ =>
      /* Queue for after reconnect */
      t.pendingMutationsRef :=
        enqueuePendingMutation(
          ~actionId,
          ~action,
          t.pendingMutationsRef.contents,
        );
      false;
    };
  };

  let sendFrame = (~frame: string, t: t) => {
    switch (t.websocketRef.contents) {
    | Some(ws) when ws->WebSocket.readyState == 1 =>
      ws->WebSocket.send_string(frame);
      true;
    | _ => false
    };
  };

  let subscribe =
      (
        ~channel: string,
        ~updatedAt: float=0.0,
        ~onOpen=() => (),
        ~onClose=() => (),
        ~onPatch,
        ~onSnapshot,
        ~onAck,
        ~onCustom=?,
        ~onMedia=?,
        ~onError=?,
        t: t,
      ) => {
    let id = t.nextIdRef.contents;
    t.nextIdRef := id + 1;

    let channelId = RealtimeClient.channelIdOfSubscription(channel);

    let callbacks = {
      id,
      subscription: channel,
      updatedAt,
      onOpen,
      onClose,
      onPatch,
      onSnapshot,
      onAck,
      onCustom,
      onMedia,
      onError,
    };

    let subs = t.subscriptionsRef.contents;
    let existing =
      switch (subs->Js.Dict.get(channelId)) {
      | Some(callbacks) => callbacks
      | None => [||]
      };
    let wasEmpty = Js.Array.length(existing) == 0;
    let alreadySelected =
      existing->Js.Array.some(~f=callback => callback.subscription == channel);
    subs->Js.Dict.set(channelId, existing->Js.Array.concat(~other=[|callbacks|]));
    t.subscriptionsRef := subs;

    /* Ensure connection is open */
    switch (t.websocketRef.contents) {
    | Some(ws) when ws->WebSocket.readyState == 1 =>
      if (wasEmpty || !alreadySelected) {
        ws->WebSocket.send_string(
          RealtimeClient.selectFrameString(channel, updatedAt),
        );
      }
      onOpen();
    | Some(_) => ()
    | None => connect(t)
    };

    {channel: channelId, id};
  };

  let unsubscribe = (t: t, handle: subscription_handle) => {
    switch (Js.Dict.get(t.subscriptionsRef.contents, handle.channel)) {
    | Some(callbacks) =>
      let remaining =
        callbacks->Js.Array.filter(~f=callback => callback.id != handle.id);
      let removed = Js.Array.length(remaining) < Js.Array.length(callbacks);
      let subs = t.subscriptionsRef.contents;
      subs->Js.Dict.set(handle.channel, remaining);
      t.subscriptionsRef := subs;

      if (removed && Js.Array.length(remaining) == 0) {
        switch (t.websocketRef.contents) {
        | Some(ws) when ws->WebSocket.readyState == 1 =>
          ws->WebSocket.send_string(unsubscribeFrameString(handle.channel))
        | _ => ()
        };
      };

      /* If no more subscriptions, close connection */
      let hasActive = () =>
        subs
        ->Js.Dict.keys
        ->Js.Array.some(~f=key => {
            switch (Js.Dict.get(subs, key)) {
            | Some(arr) when Js.Array.length(arr) > 0 => true
            | _ => false
            }
          });
      if (removed && !hasActive()) {
        clearReconnectTimeout(t);
        clearPingInterval(t);
        switch (t.websocketRef.contents) {
        | Some(ws) =>
          t.idleCloseRef := true;
          t.websocketRef := None;
          ws->WebSocket.close;
        | None => ()
        };
      };
    | None => ()
    };
  };

  let dispose = (t: t) => {
    t.disposedRef := true;
    clearReconnectTimeout(t);
    clearPingInterval(t);
    t.subscriptionsRef := Js.Dict.empty();
    switch (t.websocketRef.contents) {
    | Some(ws) =>
      t.websocketRef := None;
      ws->WebSocket.close;
    | None => ()
    };
  };
};

[@platform native]
module Multiplexed = {
  type t = unit;
  type subscription_handle = { channel: string, id: int };
  let make = (~eventUrl as _, ~baseUrl as _) => ();
  let subscribe =
      (
        ~channel as _,
        ~updatedAt as _,
        ~onOpen as _,
        ~onClose as _,
        ~onPatch as _,
        ~onSnapshot as _,
        ~onAck as _,
        ~onCustom as _=?,
        ~onMedia as _=?,
        ~onError as _=?,
        _t,
      ) => {
    channel: "",
    id: 0,
  };
  let unsubscribe = (_t, _handle) => ();
  let sendAction = (~actionId as _, ~action as _, _t) => false;
  let sendFrame = (~frame as _, _t) => false;
  let dispose = _t => ();
};
