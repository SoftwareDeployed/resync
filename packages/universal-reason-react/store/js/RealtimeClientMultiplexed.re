[@platform js]
module Multiplexed = {
  type websocket;

  type callbacks = {
    onOpen: unit => unit,
    onClose: unit => unit,
    onPatch: (~payload: StoreJson.json, ~timestamp: float) => unit,
    onSnapshot: StoreJson.json => unit,
    onAck: (string, string, option(string)) => unit,
  };

  type t = {
    websocketRef: ref(option(websocket)),
    subscriptionsRef: ref(Js.Dict.t(array(callbacks))),
    nextIdRef: ref(int),
    eventUrl: string,
    baseUrl: string,
    disposedRef: ref(bool),
    pingIntervalRef: ref(option(int)),
    reconnectTimeoutRef: ref(option(int)),
    pendingMutationsRef: ref(list((string, StoreJson.json))),
  };

  type subscription_handle = { channel: string, id: int };

  external setInterval: (unit => unit, int) => int = "setInterval";
  external clearInterval: int => unit = "clearInterval";
  external setTimeout: (unit => unit, int) => int = "setTimeout";
  external clearTimeout: int => unit = "clearTimeout";

  let make = (~eventUrl: string, ~baseUrl: string) => {
    websocketRef: ref(None),
    subscriptionsRef: ref(Js.Dict.empty()),
    nextIdRef: ref(0),
    eventUrl,
    baseUrl,
    disposedRef: ref(false),
    pingIntervalRef: ref(None),
    reconnectTimeoutRef: ref(None),
    pendingMutationsRef: ref([]),
  };

  let unsubscribeFrameString = (subscription: string) => {
    let jsonObj =
      Js.Dict.fromArray([|
        ("type", Js.Json.string("unsubscribe")),
        ("subscription", Js.Json.string(subscription)),
      |]);
    Js.Json.stringify(Js.Json.object_(jsonObj));
  };

  let rec connect = (t: t) => {
    if (!t.disposedRef.contents) {
      let url = Webapi.Url.makeWith(t.eventUrl, ~base=t.baseUrl);
      let isSecure = Webapi.Url.protocol(url) == "https:";
      url->Webapi.Url.setProtocol(isSecure ? "wss" : "ws");

      let ws: websocket = Obj.magic(WebSocket.make(url->Webapi.Url.href));
      t.websocketRef := Some(ws);

      WebSocket.onOpen(ws, () => {
        /* Send select for all active subscriptions */
        t.subscriptionsRef.contents
        ->Js.Dict.keys
        ->Js.Array.forEach(~f=channel => {
            let _ =
              t.websocketRef.contents
              |> Option.map(ws => {
                  ws->WebSocket.send_string(
                    RealtimeClient.selectFrameString(channel, 0.0),
                  )
                });
          });

        /* Send pending mutations */
        let pending = t.pendingMutationsRef.contents;
        t.pendingMutationsRef := [];
        pending |> List.iter(((actionId, action)) => {
          let _ = sendAction(~actionId, ~action, t);
        });
      });

      WebSocket.onClose(ws, () => {
        t.websocketRef := None;
        if (!t.disposedRef.contents) {
          t.reconnectTimeoutRef :=
            Some(
              setTimeout(
                () => {
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
                Js.Console.log(
                  "RealtimeClient.Multiplexed: Received snapshot",
                );
                let payload =
                  StoreJson.requiredField(
                    ~json,
                    ~fieldName="payload",
                    ~decode=value => value,
                  );
                callbacks
                ->Js.Array.forEach(~f=callback => callback.onSnapshot(payload));
              | Some("ack") =>
                Js.Console.log("RealtimeClient.Multiplexed: Received ack");
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
              | _ => ()
              }
            | None => ()
            }
          | None => Js.Console.warn("Message missing channel field")
          }
        | None => ()
        };
      });
    };
  }

  and sendAction = (~actionId: string, ~action: StoreJson.json, t: t) => {
    switch (t.websocketRef.contents) {
    | Some(ws) when ws->WebSocket.readyState == 1 =>
      ws->WebSocket.send_string(
        RealtimeClient.mutationFrameString(
          actionId,
          StoreJson.stringify(json => json, action),
        ),
      );
      true;
    | _ =>
      /* Queue for after reconnect */
      t.pendingMutationsRef :=
        [(actionId, action), ...t.pendingMutationsRef.contents];
      false;
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
        t: t,
      ) => {
    let id = t.nextIdRef.contents;
    t.nextIdRef := id + 1;

    let callbacks = {onOpen, onClose, onPatch, onSnapshot, onAck};

    /* Add/replace subscription */
    let subs = t.subscriptionsRef.contents;
    subs->Js.Dict.set(channel, [|callbacks|]);
    t.subscriptionsRef := subs;

    /* Ensure connection is open */
    switch (t.websocketRef.contents) {
    | Some(ws) when ws->WebSocket.readyState == 1 =>
      ws->WebSocket.send_string(
        RealtimeClient.selectFrameString(channel, updatedAt),
      )
    | _ => connect(t)
    };

    {channel, id};
  };

  let unsubscribe = (t: t, handle: subscription_handle) => {
    switch (Js.Dict.get(t.subscriptionsRef.contents, handle.channel)) {
    | Some(_) =>
      /* Send unsubscribe frame */
      let _ =
        t.websocketRef.contents
        |> Option.map(ws => {
            ws->WebSocket.send_string(unsubscribeFrameString(handle.channel))
          });

      /* Remove from subscriptions */
      let subs = t.subscriptionsRef.contents;
      subs->Js.Dict.set(handle.channel, [||]);
      t.subscriptionsRef := subs;

      /* If no more subscriptions, close connection */
      let hasActive =
        subs
        ->Js.Dict.keys
        ->Js.Array.some(~f=key => {
            switch (Js.Dict.get(subs, key)) {
            | Some(arr) when Js.Array.length(arr) > 0 => true
            | _ => false
            }
          });
      if (!hasActive) {
        t.disposedRef := true;
        switch (t.websocketRef.contents) {
        | Some(ws) =>
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
  type t;
  type subscription_handle = { channel: string, id: int };
  let make = (~eventUrl as _, ~baseUrl as _) => Obj.magic();
  let subscribe =
      (
        ~channel as _,
        ~updatedAt as _,
        ~onOpen as _,
        ~onClose as _,
        ~onPatch as _,
        ~onSnapshot as _,
        ~onAck as _,
        _t,
      ) => {
    channel: "",
    id: 0,
  };
  let unsubscribe = (_t, _handle) => ();
  let sendAction = (~actionId as _, ~action as _, _t) => false;
  let dispose = _t => ();
};
