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

let sendActionJson = action => {
  switch (handleRef.contents) {
  | Some(handle) =>
    let _ =
      RealtimeClientMultiplexed.Multiplexed.sendAction(
        ~actionId="",
        ~action,
        handle,
      );
    ()
  | None => ()
  };
};

let mediaAction = (~roomId, ~peerId, ~dataField, ~data) =>
  StoreJson.Object.make(dict => {
    StoreJson.Object.setString(dict, "type", "media");
    StoreJson.Object.setJson(
      dict,
      "payload",
      StoreJson.Object.make(payload => {
        StoreJson.Object.setString(payload, "room_id", roomId);
        StoreJson.Object.setString(payload, "peer_id", peerId);
        StoreJson.Object.setString(payload, dataField, data);
      }),
    );
  });

let lastFrameSent: ref(option(string)) = ref(None);

let sendMediaFrame = (roomId, peerId, frameData) => {
  switch (lastFrameSent.contents) {
  | Some(last) when last == frameData =>
    ()
  | _ =>
    lastFrameSent := Some(frameData);
    sendActionJson(mediaAction(
      ~roomId,
      ~peerId,
      ~dataField="frame_data",
      ~data=frameData,
    ));
  };
};

let sendMediaAudio = (roomId, peerId, chunkData) => {
  sendActionJson(mediaAction(
    ~roomId,
    ~peerId,
    ~dataField="chunk_data",
    ~data=chunkData,
  ));
};
