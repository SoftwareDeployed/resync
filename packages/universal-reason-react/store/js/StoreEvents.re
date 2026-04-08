type listener_id = string;

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

type listener('action) = store_event('action) => unit;
type registry('action) = ref(array((listener_id, listener('action))));

module Events = {
  let listen = (~registry: registry('action), listener: listener('action)): listener_id => {
    let listenerId = UUID.make();
    registry.contents =
      Js.Array.concat(~other=[|(listenerId, listener)|], registry.contents);
    listenerId;
  };

  let unlisten = (~registry: registry('action), listenerId: listener_id) => {
    registry.contents =
      registry.contents->Js.Array.filter(~f=((currentId, _listener)) =>
        currentId != listenerId
      );
  };

  let emit = (~registry: registry('action), event: store_event('action)) =>
    registry.contents->Js.Array.forEach(~f=((_, listener)) => listener(event));
};
