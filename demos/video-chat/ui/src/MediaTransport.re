type remote_frame = {
  frame_data: string,
  peer_id: string,
};

type remote_audio = {
  chunk_data: string,
  peer_id: string,
};

let sendRaw = dict => {
  let _ = RealtimeClient.Socket.sendFrame(Js.Json.stringify(Js.Json.object_(dict)));
  ();
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
