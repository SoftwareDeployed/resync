type realHandle = {
  stream: MediaBindings.mediaStream,
  canvas: MediaBindings.canvasElement,
  ctx: MediaBindings.canvasRenderingContext2d,
  captureVideo: MediaBindings.videoElement,
  mutable captureInterval: option(int),
  audioContext: option(MediaBindings.audioContext),
  audioSource: option(MediaBindings.audioSourceNode),
  audioProcessor: option(MediaBindings.scriptProcessorNode),
  audioGain: option(MediaBindings.gainNode),
  mutable audioEnabled: bool,
};

type mediaTrack = MediaBindings.mediaTrack;
type t =
  | Real(realHandle)
  | Override(MediaCaptureOverride.implementation, MediaCaptureOverride.handle);

let width = 640;
let height = 480;

let swallowPromise = promise => {
  promise
  |> Js.Promise.catch(_ => Js.Promise.resolve())
  |> ignore;
};

let writeAscii = (view: MediaBindings.dataView, offset: int, text: string) => {
  for (i in 0 to String.length(text) - 1) {
    MediaBindings.setUint8(view, offset + i, Char.code(String.get(text, i)));
  };
};

let wavDataUrlFromSamples = (samples: MediaBindings.float32Array, sampleRate: int) => {
  let sampleCount = MediaBindings.float32Length(samples);
  if (sampleCount == 0) {
    None;
  } else {
    let buffer = MediaBindings.makeArrayBuffer(44 + sampleCount * 2);
    let view = MediaBindings.makeDataView(buffer);
    writeAscii(view, 0, "RIFF");
    MediaBindings.setUint32(view, 4, 36 + sampleCount * 2, true);
    writeAscii(view, 8, "WAVE");
    writeAscii(view, 12, "fmt ");
    MediaBindings.setUint32(view, 16, 16, true);
    MediaBindings.setUint16(view, 20, 1, true);
    MediaBindings.setUint16(view, 22, 1, true);
    MediaBindings.setUint32(view, 24, sampleRate, true);
    MediaBindings.setUint32(view, 28, sampleRate * 2, true);
    MediaBindings.setUint16(view, 32, 2, true);
    MediaBindings.setUint16(view, 34, 16, true);
    writeAscii(view, 36, "data");
    MediaBindings.setUint32(view, 40, sampleCount * 2, true);

    let peak = ref(0.0);
    for (i in 0 to sampleCount - 1) {
      let sample = MediaBindings.float32At(samples, i);
      let clamped = Float.min(1.0, Float.max(-1.0, sample));
      let amplitude = abs_float(clamped);
      if (amplitude > peak.contents) {
        peak.contents = amplitude;
      };
      let pcm =
        if (clamped < 0.0) {
          int_of_float(clamped *. 32768.0);
        } else {
          int_of_float(clamped *. 32767.0);
        };
      MediaBindings.setInt16(view, 44 + i * 2, pcm, true);
    };

    if (peak.contents < 0.003) {
      None;
    } else {
      let bytes = MediaBindings.makeUint8Array(buffer);
      let binary = Buffer.create(MediaBindings.uint8Length(bytes));
      for (i in 0 to MediaBindings.uint8Length(bytes) - 1) {
        Buffer.add_string(binary, MediaBindings.fromCharCode(MediaBindings.uint8At(bytes, i)));
      };
      Some("data:audio/wav;base64," ++ Webapi.Base64.btoa(Buffer.contents(binary)));
    };
  };
};

[@platform js]
let createReal = () => {
  let constraints = [%obj {video: true, audio: true}];
  Js.Promise.then_(
    stream => {
      let canvasEl = Webapi.Dom.Document.createElement("canvas", Webapi.Dom.document);
      Webapi.Canvas.CanvasElement.setWidth(canvasEl, width);
      Webapi.Canvas.CanvasElement.setHeight(canvasEl, height);
      let ctx = Obj.magic(Webapi.Canvas.CanvasElement.getContext2d(canvasEl));
      let canvas = Obj.magic(canvasEl);

      let captureVideo = Obj.magic(Webapi.Dom.Document.createElement("video", Webapi.Dom.document));
      MediaBindings.setAutoplay(captureVideo, true);
      MediaBindings.setMuted(captureVideo, true);
      MediaBindings.setPlaysInline(captureVideo, true);
      MediaBindings.setVideoSrcObject(captureVideo, stream);
      swallowPromise(MediaBindings.playVideo(captureVideo));

      let audioSetup =
        if (Array.length(MediaBindings.getAudioTracks(stream)) == 0) {
          (None, None, None, None);
        } else {
          try({
            let ctx = MediaBindings.makeAudioContext([%obj {sampleRate: 16000}]);
            let source = MediaBindings.createMediaStreamSource(ctx, stream);
            let processor = MediaBindings.createScriptProcessor(ctx, 4096, 1, 1);
            let gain = MediaBindings.createGain(ctx);
            MediaBindings.setAudioParamValue(MediaBindings.gainParam(gain), 0.0);
            MediaBindings.connect(source, processor);
            MediaBindings.connect(processor, gain);
            MediaBindings.connect(gain, MediaBindings.audioContextDestination(ctx));
            if (MediaBindings.audioContextState(ctx) == "suspended") {
              swallowPromise(MediaBindings.resumeAudioContext(ctx));
            };
            (Some(ctx), Some(source), Some(processor), Some(gain));
          }) {
          | _ =>
            try({
              let ctx = MediaBindings.makeWebkitAudioContext([%obj {sampleRate: 16000}]);
              let source = MediaBindings.createMediaStreamSource(ctx, stream);
              let processor = MediaBindings.createScriptProcessor(ctx, 4096, 1, 1);
              let gain = MediaBindings.createGain(ctx);
              MediaBindings.setAudioParamValue(MediaBindings.gainParam(gain), 0.0);
              MediaBindings.connect(source, processor);
              MediaBindings.connect(processor, gain);
              MediaBindings.connect(gain, MediaBindings.audioContextDestination(ctx));
              if (MediaBindings.audioContextState(ctx) == "suspended") {
                swallowPromise(MediaBindings.resumeAudioContext(ctx));
              };
              (Some(ctx), Some(source), Some(processor), Some(gain));
            }) {
            | _ => (None, None, None, None)
            }
          }
        };

      let (audioContext, audioSource, audioProcessor, audioGain) = audioSetup;
      Js.Promise.resolve({
        stream,
        canvas,
        ctx,
        captureVideo,
        captureInterval: None,
        audioContext,
        audioSource,
        audioProcessor,
        audioGain,
        audioEnabled: true,
      });
    },
    MediaBindings.getUserMedia(
      MediaBindings.mediaDevices(Obj.magic(Webapi.Dom.Window.navigator(Webapi.Dom.window))),
      constraints,
    ),
  );
};

[@platform native]
let createReal = () => Js.Promise.resolve(Obj.magic());

[@platform js]
let create = () =>
  switch (MediaCaptureOverride.current()) {
  | Some(implementation) =>
    Js.Promise.then_(
      handle => Js.Promise.resolve(Override(implementation, handle)),
      MediaCaptureOverride.create(implementation),
    )
  | None =>
    Js.Promise.then_(
      capture => Js.Promise.resolve(Real(capture)),
      createReal(),
    )
  };

[@platform native]
let create = () => Js.Promise.resolve(Obj.magic());

[@platform js]
let attachVideoElement = (capture: t, videoEl: Dom.element) =>
  switch (capture) {
  | Real(realCapture) =>
    MediaBindings.setElementSrcObject(videoEl, realCapture.stream);
    MediaBindings.setElementMuted(videoEl, true);
    swallowPromise(MediaBindings.playElement(videoEl));
  | Override(implementation, overrideHandle) =>
    MediaCaptureOverride.attachVideoElement(implementation, overrideHandle, videoEl)
  };

[@platform native]
let attachVideoElement = (_capture, _videoEl) => ();

[@platform js]
let startCapture = (capture: t, ~onFrame, ~onAudioChunk) =>
  switch (capture) {
  | Real(realCapture) =>
    realCapture.captureInterval = Some(
      MediaBindings.setInterval(
        () => {
          try({
            MediaBindings.drawVideoImage(realCapture.ctx, realCapture.captureVideo, 0, 0, width, height);
            onFrame(MediaBindings.toDataURL(realCapture.canvas, "image/jpeg", 0.7));
          }) {
          | _ => ()
          }
        },
        33,
      )
    );

    switch (realCapture.audioProcessor, realCapture.audioContext) {
    | (Some(processor), Some(audioContext)) =>
      MediaBindings.setOnAudioProcess(processor, event => {
        if (realCapture.audioEnabled) {
          let samples = MediaBindings.getChannelData(MediaBindings.inputBuffer(event), 0);
          if (MediaBindings.float32Length(samples) > 0) {
            switch (wavDataUrlFromSamples(samples, MediaBindings.audioContextSampleRate(audioContext))) {
            | Some(wavDataUrl) => onAudioChunk(wavDataUrl)
            | None => ()
            };
          };
        };
      })
    | _ => ()
    }
  | Override(implementation, overrideHandle) =>
    MediaCaptureOverride.startCapture(
      implementation,
      overrideHandle,
      MediaCaptureOverride.makeStartCaptureCallbacks(~onFrame, ~onAudioChunk, ()),
    )
  };

[@platform native]
let startCapture = (_capture, ~onFrame: _, ~onAudioChunk: _) => ();

[@platform js]
let stopCapture = (capture: t) =>
  switch (capture) {
  | Real(realCapture) =>
    switch (realCapture.captureInterval) {
    | Some(intervalId) =>
      MediaBindings.clearInterval(intervalId);
      realCapture.captureInterval = None;
    | None => ()
    };

    switch (realCapture.audioProcessor) {
    | Some(processor) =>
      try({
        MediaBindings.disconnect(processor);
      }) {
      | _ => ()
      }
    | None => ()
    };

    switch (realCapture.audioSource) {
    | Some(source) =>
      try({
        MediaBindings.disconnect(source);
      }) {
      | _ => ()
      }
    | None => ()
    };

    switch (realCapture.audioGain) {
    | Some(gain) =>
      try({
        MediaBindings.disconnect(gain);
      }) {
      | _ => ()
      }
    | None => ()
    };

    switch (realCapture.audioContext) {
    | Some(audioContext) => swallowPromise(MediaBindings.closeAudioContext(audioContext))
    | None => ()
    };

    realCapture.stream
    ->MediaBindings.getTracks
    ->Js.Array.forEach(~f=track => {
        MediaBindings.setEnabled(track, false);
        MediaBindings.stopTrack(track);
      })
  | Override(implementation, overrideHandle) =>
    MediaCaptureOverride.stopCapture(implementation, overrideHandle)
  };

[@platform native]
let stopCapture = _capture => ();

[@platform js]
let getVideoTracks = capture =>
  switch (capture) {
  | Real(realCapture) => MediaBindings.getVideoTracks(realCapture.stream)
  | Override(_, _) => [||]
  };

[@platform native]
let getVideoTracks = _capture => [||];

[@platform js]
let getAudioTracks = capture =>
  switch (capture) {
  | Real(realCapture) => MediaBindings.getAudioTracks(realCapture.stream)
  | Override(_, _) => [||]
  };

[@platform native]
let getAudioTracks = _capture => [||];

[@platform js]
let setAudioCaptureEnabled = (capture: t, enabled: bool) =>
  switch (capture) {
  | Real(realCapture) => realCapture.audioEnabled = enabled
  | Override(implementation, overrideHandle) =>
    MediaCaptureOverride.setAudioEnabled(implementation, overrideHandle, enabled)
  };

[@platform native]
let setAudioCaptureEnabled = (_capture, _enabled) => ();

[@platform js]
let setVideoEnabled = (capture, enabled) =>
  switch (capture) {
  | Real(realCapture) => {
      let tracks = MediaBindings.getVideoTracks(realCapture.stream);
      Js.Array.forEach(~f=track => MediaBindings.setEnabled(track, enabled), tracks);
    }
  | Override(implementation, overrideHandle) =>
    MediaCaptureOverride.setVideoEnabled(implementation, overrideHandle, enabled)
  };

[@platform native]
let setVideoEnabled = (_capture, _enabled) => ();

[@platform js]
let setAudioEnabled = (capture, enabled) =>
  switch (capture) {
  | Real(realCapture) => {
      setAudioCaptureEnabled(capture, enabled);
      let tracks = MediaBindings.getAudioTracks(realCapture.stream);
      Js.Array.forEach(~f=track => MediaBindings.setEnabled(track, enabled), tracks);
    }
  | Override(implementation, overrideHandle) =>
    MediaCaptureOverride.setAudioEnabled(implementation, overrideHandle, enabled)
  };

[@platform native]
let setAudioEnabled = (_capture, _enabled) => ();
