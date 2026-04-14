type remote_frame = {
  frame_data: string,
  peer_id: string,
};

type remote_audio = {
  chunk_data: string,
  peer_id: string,
};

/* Transport-specific handle ref. This is NOT singleton active connection state;
it's the transport layer's local reference to the current connection handle.
The handle is owned by the store runtime and set via setHandle when connected. */
let handleRef: ref(option(RealtimeClientMultiplexed.Multiplexed.t)) = ref(None);

/* Set the current connection handle. Called by store runtime on connect. */
let setHandle = (handle: option(RealtimeClientMultiplexed.Multiplexed.t)) => {
  handleRef := handle;
};

let sendRaw = dict => {
  let frame = Js.Json.stringify(Js.Json.object_(dict));
  switch (handleRef.contents) {
  | Some(handle) =>
    let _ = RealtimeClientMultiplexed.Multiplexed.sendAction(~actionId="", ~action=StoreJson.parse(frame), handle);
    ()
  | None => ()
  };
};

let lastFrameSent: ref(option(string)) = ref(None);

let sendMediaFrame = (roomId, peerId, frameData) => {
  switch (lastFrameSent.contents) {
  | Some(last) when last == frameData =>
    ()
  | _ =>
    lastFrameSent := Some(frameData);
    let payload = Js.Dict.empty();
    Js.Dict.set(payload, "room_id", Js.Json.string(roomId));
    Js.Dict.set(payload, "peer_id", Js.Json.string(peerId));
    Js.Dict.set(payload, "frame_data", Js.Json.string(frameData));
    let msg = Js.Dict.empty();
    Js.Dict.set(msg, "type", Js.Json.string("media"));
    Js.Dict.set(msg, "payload", Js.Json.object_(payload));
    sendRaw(msg);
  };
};

let sendMediaAudio = (roomId, peerId, chunkData) => {
  let payload = Js.Dict.empty();
  Js.Dict.set(payload, "room_id", Js.Json.string(roomId));
  Js.Dict.set(payload, "peer_id", Js.Json.string(peerId));
  Js.Dict.set(payload, "chunk_data", Js.Json.string(chunkData));
  let msg = Js.Dict.empty();
  Js.Dict.set(msg, "type", Js.Json.string("media"));
  Js.Dict.set(msg, "payload", Js.Json.object_(payload));
  sendRaw(msg);
};
