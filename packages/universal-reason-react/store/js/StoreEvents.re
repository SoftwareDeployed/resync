type listener_id = string;
type callback('payload) = 'payload => unit;
type callback_registry('payload) = ref(array((listener_id, callback('payload))));

module Callback = {
  let listen =
      (
        ~registry: callback_registry('payload),
        listener: callback('payload),
      )
      : listener_id => {
    let listenerId = UUID.make();
    registry.contents =
      registry.contents->Js.Array.concat(~other=[|(listenerId, listener)|]);
    listenerId;
  };

  let unlisten = (~registry: callback_registry('payload), listenerId: listener_id) => {
    registry.contents =
      registry.contents->Js.Array.filter(~f=((currentId, _listener)) =>
        currentId != listenerId
      );
  };

  let emit = (~registry: callback_registry('payload), payload: 'payload) =>
    registry.contents->Js.Array.forEach(~f=((_, listener)) => listener(payload));
};

type action_ack('action) = {
  actionId: string,
  action: option('action),
};

type action_failure('action) = {
  actionId: string,
  action: option('action),
  message: string,
};

/* Narrow synced runtime events. These model store-level outcomes, not raw
   websocket frames. Snapshot and patch transport details stay internal to the
   runtime so reducers remain pure and observers attach at the store layer. */
type store_event('action) =
  | Open
  | Close
  | Reconnect
  | ActionAcked(action_ack('action))
  | ActionFailed(action_failure('action))
  | ConnectionError(string)
  | CustomEvent(StoreJson.json)
  | MediaEvent(StoreJson.json);

type listener('action) = callback(store_event('action));
type registry('action) = callback_registry(store_event('action));

module Events = {
  let listen = (~registry: registry('action), listener: listener('action)): listener_id =>
    Callback.listen(~registry, listener);

  let unlisten = (~registry: registry('action), listenerId: listener_id) =>
    Callback.unlisten(~registry, listenerId);

  let emit = (~registry: registry('action), event: store_event('action)) =>
    Callback.emit(~registry, event);
};
