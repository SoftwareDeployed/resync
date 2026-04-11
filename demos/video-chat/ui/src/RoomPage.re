open Tilia.React;

[@platform js]
let drawVideoFrame = (canvas: Dom.element, dataUrl: string) => {
  let ctx = Obj.magic(Webapi.Canvas.CanvasElement.getContext2d(canvas));
  let htmlImage = Webapi.Dom.HtmlImageElement.make();
  Webapi.Dom.HtmlImageElement.addLoadEventListener(_event => {
    let nextWidth = Webapi.Dom.HtmlImageElement.width(htmlImage);
    let nextHeight = Webapi.Dom.HtmlImageElement.height(htmlImage);
    if (Webapi.Canvas.CanvasElement.width(canvas) != nextWidth) {
      Webapi.Canvas.CanvasElement.setWidth(canvas, nextWidth);
    };
    if (Webapi.Canvas.CanvasElement.height(canvas) != nextHeight) {
      Webapi.Canvas.CanvasElement.setHeight(canvas, nextHeight);
    };
    MediaBindings.drawImage(ctx, Obj.magic(htmlImage), 0, 0);
  }, htmlImage);
  Webapi.Dom.HtmlImageElement.setSrc(htmlImage, dataUrl);
};

[@platform native]
let drawVideoFrame = (_canvas, _data) => ();

[@platform js]
let playAudioChunk = (chunk: string) => {
  let audio = MediaBindings.makeAudio(chunk);
  MediaBindings.playAudio(audio)
  |> Js.Promise.catch(_ => Js.Promise.resolve())
  |> ignore;
};

[@platform native]
let playAudioChunk = (_chunk: string) => ();

[@platform js]
let getInputValue = event => React.Event.Form.target(event)##value;

[@platform native]
let getInputValue = _event => "";

[@platform js]
let scrollToBottom = elOpt => {
  switch (elOpt) {
  | Some(el) =>
    let el = Obj.magic(el);
    el##scrollTop #= el##scrollHeight;
  | None => ()
  };
};

[@platform native]
let scrollToBottom = _elOpt => ();

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

      let releaseCapture = () => {
        switch (captureRef.current) {
        | Some(capture) =>
          MediaCapture.stopCapture(capture);
          captureRef.current = None;
        | None => ()
        };
      };

      /* Media event listener - lifecycle tied via Events.listen/unlisten */
      React.useEffect0(() => {
        let listenerId =
          VideoChatStore.Events.listen(event => {
            switch (event) {
            | MediaEvent(json) =>
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
            | _ => ()
            }
          });
        Some(() => VideoChatStore.Events.unlisten(listenerId));
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
                  },
                500,
              );

            Some(
              () => {
                Js.Global.clearTimeout(timeoutId);
                VideoChatStore.leaveRoom(store);
              },
            );
          },
        [|roomIdValue|],
      );

      React.useEffect1(
        () => {
          if (store.state.is_joined) {
            releaseCapture();
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
            Some(
              () => releaseCapture(),
            );
          } else {
            None;
          };
        },
        [|store.state.is_joined|],
      );

      let leaveRoom = () => {
        releaseCapture();
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

      let messagesContainerRef = React.useRef(None);

      let (chatText, setChatText) = React.useState(() => "");

      let handleSendMessage = () => {
        if (chatText != "") {
          VideoChatStore.sendMessage(store, chatText);
          setChatText(_ => "");
        };
      };

      React.useEffect1(
        () => {
          scrollToBottom(messagesContainerRef.current);
          None;
        },
        [|Array.length(store.state.messages)|],
      );

      <div id="room-page" className="min-h-screen bg-gray-900 p-4">
        <div className="mx-auto max-w-6xl">
          <div className="mb-4 flex items-center justify-between">
            <div className="text-white">
              <h1 id="room-heading" className="text-xl font-bold">
                {React.string("Room: " ++ roomIdValue)}
              </h1>
              <p id="room-peer-count" className="text-sm text-gray-400">
                {React.string("Peers: " ++ string_of_int(store.peers_count))}
              </p>
            </div>
            <div className="flex items-center gap-2">
              <button
                id="toggle-video-button"
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
                id="toggle-audio-button"
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
                id="leave-room-button"
                onClick={_ => leaveRoom()}
                className="flex items-center gap-2 rounded-lg bg-red-600 px-4 py-2 text-white hover:bg-red-700">
                <Lucide.IconPhoneOff size=16 />
                {React.string("Leave")}
              </button>
            </div>
          </div>
          <div className="grid grid-cols-1 gap-4 lg:grid-cols-3">
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
                      id="remote-video-paused-overlay"
                      className="absolute inset-0 flex flex-col items-center justify-center bg-gray-800">
                      <Lucide.IconVideoOff size=48 />
                     <p className="mt-2 text-gray-400">
                       {React.string("Video paused")}
                     </p>
                   </div>
                 : React.null}
            </div>
            <div className="flex h-96 flex-col rounded-xl bg-gray-800 p-4 lg:h-auto">
              <h2 className="mb-2 text-lg font-semibold text-white">
                {React.string("Chat")}
              </h2>
              <div
                ref={ReactDOM.Ref.callbackDomRef(elem => {
                  messagesContainerRef.current = (
                    switch (Js.Nullable.toOption(elem)) {
                    | Some(e) => Some(e)
                    | None => None
                    }
                  )
                })}
                className="flex-1 overflow-y-auto rounded-lg bg-gray-900 p-2">
                {store.state.messages
                 ->Js.Array.map(
                     ~f=(msg: Model.ChatMessage.t) =>
                       <div key={msg.id} className="mb-2">
                         <span className="text-xs font-semibold text-blue-400">
                           {React.string(
                              msg.sender_id == store.state.client_id
                                ? "You" : "Peer " ++ msg.sender_id,
                            )}
                         </span>
                         <p className="text-sm text-gray-200">
                           {React.string(msg.text)}
                         </p>
                       </div>,
                     _,
                   )
                 ->React.array}
              </div>
              <div className="mt-2 flex gap-2">
                <input
                  id="chat-input"
                  type_="text"
                  value=chatText
                  onChange={event => {
                    let value = getInputValue(event);
                    setChatText(_ => value);
                  }}
                  className="flex-1 rounded-lg bg-gray-700 px-3 py-2 text-white placeholder-gray-400 focus:outline-none focus:ring-2 focus:ring-blue-500"
                  placeholder="Type a message..."
                />
                <button
                  id="chat-send"
                  onClick={_ => handleSendMessage()}
                  className="rounded-lg bg-blue-600 px-4 py-2 text-white hover:bg-blue-700">
                  {React.string("Send")}
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>;
    });
};

let make = (~params, ~searchParams, ()) => <View params searchParams />;
