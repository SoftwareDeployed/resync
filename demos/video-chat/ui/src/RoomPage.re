open Tilia.React;

[@platform js]
let drawVideoFrame: (Dom.element, string) => unit = [%raw
  {|
  function(canvas, dataUrl) {
    if (!canvas || canvas.tagName !== 'CANVAS') return;
    var ctx = canvas.getContext('2d');
    var img = new Image();
    img.onload = function() {
      // Resize canvas to match image if needed
      if (canvas.width !== img.width || canvas.height !== img.height) {
        canvas.width = img.width;
        canvas.height = img.height;
      }
      ctx.drawImage(img, 0, 0);
    };
    img.src = dataUrl;
  }
  |}
];

[@platform native]
let drawVideoFrame = (_canvas, _data) => ();

[@platform js]
let playAudioChunk: string => unit = [%raw
  {|
  function(chunk) {
    var audio = new Audio(chunk);
    audio.play().catch(function() {});
  }
  |}
];

[@platform native]
let playAudioChunk = (_chunk: string) => ();

module View = {
  [@react.component]
  let make =
    leaf(
      (
        ~params: UniversalRouter.Params.t,
        ~searchParams: UniversalRouter.SearchParams.t,
      ) => {
      let _searchParams: UniversalRouter.SearchParams.t = searchParams;
      let roomId = UniversalRouter.Params.find("roomId", params);
      let roomIdValue = Option.value(roomId, ~default="");
      let store = VideoChatStore.Context.useStore();
      let router = UniversalRouter.useRouter();

      let localVideoRef = React.useRef(None);
      let remoteCanvasRef = React.useRef(None);
      let captureRef = React.useRef(None);
      let remotePeerRef = React.useRef(None);

      React.useEffect0(() => {
        VideoChatStore.registerMediaHandler(json => {
          // Media messages are wrapped: {"type": "media", "payload": {...}}
          let payload =
            StoreJson.optionalField(~json, ~fieldName="payload", ~decode=value =>
              value
            );
          let actualPayload =
            switch (payload) {
            | Some(p) => p
            | None => json
            };
          let frameData =
            StoreJson.optionalField(
              ~json=actualPayload,
              ~fieldName="frame_data",
              ~decode=Melange_json.Primitives.string_of_json,
            );
          let peerId =
            StoreJson.optionalField(
              ~json=actualPayload,
              ~fieldName="peer_id",
              ~decode=Melange_json.Primitives.string_of_json,
            );
          let roomId =
            StoreJson.optionalField(
              ~json=actualPayload,
              ~fieldName="room_id",
              ~decode=Melange_json.Primitives.string_of_json,
            );
          switch (frameData, peerId, roomId) {
          | (Some(data), Some(pid), Some(_rid)) =>
            remotePeerRef.current = Some(pid);
            switch (remoteCanvasRef.current) {
            | Some(canvas) => drawVideoFrame(canvas, data)
            | None => ()
            };
            ();
          | _ => ()
          };
          let chunkData =
            StoreJson.optionalField(
              ~json=actualPayload,
              ~fieldName="chunk_data",
              ~decode=Melange_json.Primitives.string_of_json,
            );
          switch (chunkData) {
          | Some(data) => playAudioChunk(data)
          | None => ()
          };
        });
        Some(() => ());
      });

      React.useEffect1(
        () =>
          if (roomIdValue == "") {
            router.push("/");
            None;
          } else {
            let timeoutId =
              Js.Global.setTimeout(
                ~f=
                  () => {
                    let peerId = VideoChatStore.joinRoom(store, roomIdValue);
                    Js.Console.log2("Joined room with peerId:", peerId);

                    let _ =
                      Js.Promise.then_(
                        capture => {
                          captureRef.current = Some(capture);
                          switch (localVideoRef.current) {
                          | Some(videoEl) =>
                            MediaCapture.attachVideoElement(capture, videoEl)
                          | None => ()
                          };
                          MediaCapture.startCapture(
                            capture,
                            ~onFrame=
                              frameData =>
                                MediaTransport.sendMediaFrame(
                                  roomIdValue,
                                  store.state.client_id,
                                  frameData,
                                ),
                            ~onAudioChunk=
                              chunkData =>
                                MediaTransport.sendMediaAudio(
                                  roomIdValue,
                                  store.state.client_id,
                                  chunkData,
                                ),
                          );
                          Js.Promise.resolve();
                        },
                        MediaCapture.create(),
                      );
                    ();
                  },
                500,
              );

            Some(
              () => {
                Js.Global.clearTimeout(timeoutId);
                switch (captureRef.current) {
                | Some(capture) => MediaCapture.stopCapture(capture)
                | None => ()
                };
                VideoChatStore.leaveRoom(store);
              },
            );
          },
        [|roomIdValue|],
      );

      let leaveRoom = () => {
        switch (captureRef.current) {
        | Some(capture) => MediaCapture.stopCapture(capture)
        | None => ()
        };
        VideoChatStore.leaveRoom(store);
        router.push("/");
      };

      let toggleVideo = () => {
        let nextEnabled = !store.state.local_video_enabled;
        switch (captureRef.current) {
        | Some(capture) => MediaCapture.setVideoEnabled(capture, nextEnabled)
        | None => ()
        };
        VideoChatStore.toggleVideo(store, nextEnabled);
      };

      let toggleAudio = () => {
        let nextEnabled = !store.state.local_audio_enabled;
        switch (captureRef.current) {
        | Some(capture) => MediaCapture.setAudioEnabled(capture, nextEnabled)
        | None => ()
        };
        VideoChatStore.toggleAudio(store, nextEnabled);
      };

      let hasRemotePeer =
        switch (store.state.remote_peer_id) {
        | Some(_) => true
        | None => false
        };

      <div className="min-h-screen bg-gray-900 p-4">
        <div className="mx-auto max-w-6xl">
          <div className="mb-4 flex items-center justify-between">
            <div className="text-white">
              <h1 className="text-xl font-bold">
                {React.string("Room: " ++ roomIdValue)}
              </h1>
              <p className="text-sm text-gray-400">
                {React.string("Peers: " ++ string_of_int(store.peers_count))}
              </p>
            </div>
            <div className="flex items-center gap-2">
              <button
                onClick={_ => toggleVideo()}
                className={
                  "flex items-center gap-2 rounded-lg px-4 py-2 text-white "
                  ++ (
                    store.state.local_video_enabled
                      ? "bg-blue-600 hover:bg-blue-700"
                      : "bg-gray-600 hover:bg-gray-700"
                  )
                }>
                <Lucide.IconVideo size=16 />
                {React.string(
                   store.state.local_video_enabled ? "Video On" : "Video Off",
                 )}
              </button>
              <button
                onClick={_ => toggleAudio()}
                className={
                  "flex items-center gap-2 rounded-lg px-4 py-2 text-white "
                  ++ (
                    store.state.local_audio_enabled
                      ? "bg-blue-600 hover:bg-blue-700"
                      : "bg-gray-600 hover:bg-gray-700"
                  )
                }>
                <Lucide.IconMic size=16 />
                {React.string(
                   store.state.local_audio_enabled ? "Audio On" : "Audio Off",
                 )}
              </button>
              <button
                onClick={_ => leaveRoom()}
                className="flex items-center gap-2 rounded-lg bg-red-600 px-4 py-2 text-white hover:bg-red-700">
                <Lucide.IconPhoneOff size=16 />
                {React.string("Leave")}
              </button>
            </div>
          </div>
          <div className="grid grid-cols-1 gap-4 md:grid-cols-2">
            <div
              className="relative aspect-video overflow-hidden rounded-xl bg-gray-800">
              <video
                ref={ReactDOM.Ref.callbackDomRef(elem => {
                  localVideoRef.current = (
                    switch (Js.Nullable.toOption(elem)) {
                    | Some(e) => Some(e)
                    | None => None
                    }
                  )
                })}
                className="h-full w-full object-cover"
                autoPlay=true
                muted=true
                playsInline=true
              />
            </div>
            <div
              className="relative aspect-video overflow-hidden rounded-xl bg-gray-800 flex flex-col items-center justify-center">
              <div
                className={
                  "flex flex-col items-center justify-center"
                  ++ (hasRemotePeer ? " hidden" : "")
                }>
                <Lucide.IconUser size=48 />
                <p className="mt-2 text-gray-400">
                  {React.string("Remote video")}
                </p>
                <p className="mt-1 text-xs text-yellow-400">
                  {React.string("Waiting")}
                </p>
              </div>
              <canvas
                ref={ReactDOM.Ref.callbackDomRef(elem => {
                  remoteCanvasRef.current = (
                    switch (Js.Nullable.toOption(elem)) {
                    | Some(e) => Some(e)
                    | None => None
                    }
                  )
                })}
                className={
                  "h-full w-full object-cover"
                  ++ (hasRemotePeer ? "" : " hidden")
                }
                width="640"
                height="480"
              />
              {!store.state.remote_video_enabled && hasRemotePeer
                 ? <div
                     className="absolute inset-0 flex flex-col items-center justify-center bg-gray-800">
                     <Lucide.IconVideoOff size=48 />
                     <p className="mt-2 text-gray-400">
                       {React.string("Video paused")}
                     </p>
                   </div>
                 : React.null}
            </div>
          </div>
        </div>
      </div>;
    });
};

let make = (~params, ~searchParams, ()) => <View params searchParams />;
