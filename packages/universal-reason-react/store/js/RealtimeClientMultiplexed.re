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
    reconnectTimeoutRef: ref(option(int)),
    pendingMutationsRef: ref(array(pending_mutation)),
  };

  type subscription_handle = { channel: string, id: int };

  external setTimeout: (unit => unit, int) => int = "setTimeout";
  external clearTimeout: int => unit = "clearTimeout";

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
    reconnectTimeoutRef: ref(None),
    pendingMutationsRef: ref([||]),
  };

  let unsubscribeFrameString = (subscription: string) =>
    StoreJson.stringify(
      json => json,
      StoreJson.Object.make(dict => {
        StoreJson.Object.setString(dict, "type", "unsubscribe");
        StoreJson.Object.setString(dict, "channel", subscription);
      }),
    );

  let rec connect = (t: t) => {
    if (!t.disposedRef.contents) {
      t.idleCloseRef := false;
      let url = Webapi.Url.makeWith(t.eventUrl, ~base=t.baseUrl);
      let isSecure = Webapi.Url.protocol(url) == "https:";
      url->Webapi.Url.setProtocol(isSecure ? "wss" : "ws");

      let ws: websocket = WebSocket.make(url->Webapi.Url.href);
      t.websocketRef := Some(ws);

      WebSocket.onOpen(ws, () => {
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
      });

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
        };
        if (isCurrentSocket && !t.disposedRef.contents && !wasIdleClose) {
          t.reconnectTimeoutRef :=
            Some(
              setTimeout(
                () => {
                  t.reconnectTimeoutRef := None;
                  connect(t);
                },
                250,
              ),
            );
        };
      });

      WebSocket.onMessage(ws, event => {
        let data: string = event##data;
        switch (StoreJson.tryParse(data)) {
        | Some(json) =>
          switch (
            StoreJson.field(json, "channel")
            |> Option.map(Melange_json.Primitives.string_of_json)
          ) {
          | Some(channel) =>
            switch (Js.Dict.get(t.subscriptionsRef.contents, channel)) {
            | Some(callbacks) =>
              /* Route message to this channel's callbacks */
              switch (
                StoreJson.field(json, "type")
                |> Option.map(Melange_json.Primitives.string_of_json)
              ) {
              | Some("patch") =>
                let payload =
                  StoreJson.requiredField(
                    ~json,
                    ~fieldName="payload",
                    ~decode=value => value,
                  );
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
                callbacks
                ->Js.Array.forEach(~f=callback => callback.onSnapshot(payload));
              | Some("ack") =>
                let actionId =
                  StoreJson.requiredField(
                    ~json,
                    ~fieldName="actionId",
                    ~decode=Melange_json.Primitives.string_of_json,
                  );
                let status =
                  StoreJson.requiredField(
                    ~json,
                    ~fieldName="status",
                    ~decode=Melange_json.Primitives.string_of_json,
                  );
                let error =
                  StoreJson.optionalField(
                    ~json,
                    ~fieldName="error",
                    ~decode=Melange_json.Primitives.string_of_json,
                  );
                callbacks
                ->Js.Array.forEach(
                    ~f=callback => callback.onAck(actionId, status, error),
                  );
              | Some("custom") =>
                let payload =
                  StoreJson.requiredField(
                    ~json,
                    ~fieldName="payload",
                    ~decode=value => value,
                  );
                callbacks
                ->Js.Array.forEach(~f=callback =>
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
                callbacks
                ->Js.Array.forEach(~f=callback =>
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
                callbacks
                ->Js.Array.forEach(~f=callback =>
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
        | None => ()
        };
      });
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
        let _ =
          t.websocketRef.contents
          |> Option.map(ws => {
              ws->WebSocket.send_string(unsubscribeFrameString(handle.channel))
            });
        ();
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
