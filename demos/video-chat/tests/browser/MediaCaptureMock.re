type startCaptureCallbacks;

type handle = {
  mutable frameIntervalId: option(int),
  mutable audioIntervalId: option(int),
  mutable videoEnabled: bool,
  mutable audioEnabled: bool,
};

type state = {
  mutable frameData: string,
  mutable audioChunkData: string,
  mutable emitAudio: bool,
  mutable frameIntervalMs: int,
  mutable audioIntervalMs: int,
  mutable createCount: int,
  mutable startCount: int,
  mutable stopCount: int,
  mutable lastVideoEnabled: bool,
  mutable lastAudioEnabled: bool,
};

type config;

[@mel.get]
external configFrameData: config => Js.Nullable.t(string) = "frameData";

[@mel.get]
external configAudioChunkData: config => Js.Nullable.t(string) = "audioChunkData";

[@mel.get]
external configEmitAudio: config => Js.Nullable.t(bool) = "emitAudio";

[@mel.get]
external configFrameIntervalMs: config => Js.Nullable.t(int) = "frameIntervalMs";

[@mel.get]
external configAudioIntervalMs: config => Js.Nullable.t(int) = "audioIntervalMs";

type snapshot;

[@mel.obj]
external makeSnapshot:
  (
    ~frameData: string,
    ~audioChunkData: string,
    ~emitAudio: bool,
    ~frameIntervalMs: int,
    ~audioIntervalMs: int,
    ~createCount: int,
    ~startCount: int,
    ~stopCount: int,
    ~lastVideoEnabled: bool,
    ~lastAudioEnabled: bool,
    unit,
  ) =>
  snapshot = "";

type controller;

[@mel.obj]
external makeController:
  (~reset: unit => unit, ~configure: config => unit, ~snapshot: unit => snapshot, unit) => controller = "";

type implementation;

[@mel.obj]
external makeImplementation:
  (
    ~create: unit => Js.Promise.t(handle),
    ~attachVideoElement: (handle, 'element) => unit,
    ~startCapture: (handle, startCaptureCallbacks) => unit,
    ~stopCapture: handle => unit,
    ~setVideoEnabled: (handle, bool) => unit,
    ~setAudioEnabled: (handle, bool) => unit,
    unit,
  ) =>
  implementation = "";

[@mel.send]
external callOnFrame: (startCaptureCallbacks, string) => unit = "onFrame";

[@mel.send]
external callOnAudioChunk: (startCaptureCallbacks, string) => unit = "onAudioChunk";

external globalThis: 'a = "globalThis";

external setInterval: (unit => unit, int) => int = "setInterval";

external clearInterval: int => unit = "clearInterval";

[@mel.set]
external setOverride: ('a, 'b) => unit = "__RESYNC_VIDEO_CHAT_MEDIA_CAPTURE_OVERRIDE";

[@mel.set]
external setController: ('a, 'b) => unit = "__RESYNC_VIDEO_CHAT_MEDIA_CAPTURE_MOCK";

let defaultFrameData =
  "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='640' height='480' viewBox='0 0 640 480'><rect width='640' height='480' fill='%23111827'/><text x='320' y='240' fill='white' font-size='28' text-anchor='middle' dominant-baseline='middle'>mock-frame</text></svg>";

let defaultAudioChunkData =
  "data:audio/wav;base64,UklGRjYAAABXQVZFZm10IBAAAAABAAEAgD4AAAB9AAACABAAZGF0YRIAAAAAAA==";

let state = {
  frameData: defaultFrameData,
  audioChunkData: defaultAudioChunkData,
  emitAudio: false,
  frameIntervalMs: 33,
  audioIntervalMs: 120,
  createCount: 0,
  startCount: 0,
  stopCount: 0,
  lastVideoEnabled: true,
  lastAudioEnabled: true,
};

let clearHandleIntervals = handle => {
  switch (handle.frameIntervalId) {
  | Some(intervalId) =>
    clearInterval(intervalId);
    handle.frameIntervalId = None;
  | None => ()
  };

  switch (handle.audioIntervalId) {
  | Some(intervalId) =>
    clearInterval(intervalId);
    handle.audioIntervalId = None;
  | None => ()
  };
};

let reset = () => {
  state.frameData = defaultFrameData;
  state.audioChunkData = defaultAudioChunkData;
  state.emitAudio = false;
  state.frameIntervalMs = 33;
  state.audioIntervalMs = 120;
  state.createCount = 0;
  state.startCount = 0;
  state.stopCount = 0;
  state.lastVideoEnabled = true;
  state.lastAudioEnabled = true;
};

let configure = (~frameData=?, ~audioChunkData=?, ~emitAudio=?, ~frameIntervalMs=?, ~audioIntervalMs=?, ()) => {
  switch (frameData) {
  | Some(nextFrameData) => state.frameData = nextFrameData
  | None => ()
  };

  switch (audioChunkData) {
  | Some(nextAudioChunkData) => state.audioChunkData = nextAudioChunkData
  | None => ()
  };

  switch (emitAudio) {
  | Some(nextEmitAudio) => state.emitAudio = nextEmitAudio
  | None => ()
  };

  switch (frameIntervalMs) {
  | Some(nextFrameIntervalMs) => state.frameIntervalMs = nextFrameIntervalMs
  | None => ()
  };

  switch (audioIntervalMs) {
  | Some(nextAudioIntervalMs) => state.audioIntervalMs = nextAudioIntervalMs
  | None => ()
  };
};

let configureFromJs = config => {
  switch (Js.Nullable.toOption(configFrameData(config))) {
  | Some(frameData) => state.frameData = frameData
  | None => ()
  };

  switch (Js.Nullable.toOption(configAudioChunkData(config))) {
  | Some(audioChunkData) => state.audioChunkData = audioChunkData
  | None => ()
  };

  switch (Js.Nullable.toOption(configEmitAudio(config))) {
  | Some(emitAudio) => state.emitAudio = emitAudio
  | None => ()
  };

  switch (Js.Nullable.toOption(configFrameIntervalMs(config))) {
  | Some(frameIntervalMs) => state.frameIntervalMs = frameIntervalMs
  | None => ()
  };

  switch (Js.Nullable.toOption(configAudioIntervalMs(config))) {
  | Some(audioIntervalMs) => state.audioIntervalMs = audioIntervalMs
  | None => ()
  };
};

let snapshot = () =>
  makeSnapshot(
    ~frameData=state.frameData,
    ~audioChunkData=state.audioChunkData,
    ~emitAudio=state.emitAudio,
    ~frameIntervalMs=state.frameIntervalMs,
    ~audioIntervalMs=state.audioIntervalMs,
    ~createCount=state.createCount,
    ~startCount=state.startCount,
    ~stopCount=state.stopCount,
    ~lastVideoEnabled=state.lastVideoEnabled,
    ~lastAudioEnabled=state.lastAudioEnabled,
    (),
  );

let create = () => {
  state.createCount = state.createCount + 1;
  state.lastVideoEnabled = true;
  state.lastAudioEnabled = true;
  Js.Promise.resolve({
    frameIntervalId: None,
    audioIntervalId: None,
    videoEnabled: true,
    audioEnabled: true,
  });
};

let attachVideoElement = (_handle, _videoEl) => ();

let startCapture = (handle, callbacks) => {
  state.startCount = state.startCount + 1;
  clearHandleIntervals(handle);

  handle.frameIntervalId =
    Some(
      setInterval(
        () => {
          if (handle.videoEnabled) {
            callOnFrame(callbacks, state.frameData);
          };
        },
        state.frameIntervalMs,
      )
    );

  if (state.emitAudio) {
    handle.audioIntervalId =
      Some(
        setInterval(
          () => {
            if (handle.audioEnabled) {
              callOnAudioChunk(callbacks, state.audioChunkData);
            };
          },
          state.audioIntervalMs,
        )
      );
  };
};

let stopCapture = handle => {
  state.stopCount = state.stopCount + 1;
  clearHandleIntervals(handle);
};

let setVideoEnabled = (handle, enabled) => {
  handle.videoEnabled = enabled;
  state.lastVideoEnabled = enabled;
};

let setAudioEnabled = (handle, enabled) => {
  handle.audioEnabled = enabled;
  state.lastAudioEnabled = enabled;
};

let installScriptSource = {mock|
(() => {
  if (globalThis.DOMStringList && !globalThis.DOMStringList.prototype.includes) {
    globalThis.DOMStringList.prototype.includes = function(name) {
      if (typeof this.contains === 'function') {
        return this.contains(name);
      }

      return Array.from(this).includes(name);
    };
  }

  const defaultFrameData = "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='640' height='480' viewBox='0 0 640 480'><rect width='640' height='480' fill='%23111827'/><text x='320' y='240' fill='white' font-size='28' text-anchor='middle' dominant-baseline='middle'>mock-frame</text></svg>";
  const defaultAudioChunkData = "data:audio/wav;base64,UklGRjYAAABXQVZFZm10IBAAAAABAAEAgD4AAAB9AAACABAAZGF0YRIAAAAAAA==";
  const state = {
    frameData: defaultFrameData,
    audioChunkData: defaultAudioChunkData,
    emitAudio: false,
    frameIntervalMs: 33,
    audioIntervalMs: 120,
    createCount: 0,
    startCount: 0,
    stopCount: 0,
    lastVideoEnabled: true,
    lastAudioEnabled: true,
  };

  const clearHandleIntervals = handle => {
    if (handle.frameIntervalId != null) {
      globalThis.clearInterval(handle.frameIntervalId);
      handle.frameIntervalId = null;
    }

    if (handle.audioIntervalId != null) {
      globalThis.clearInterval(handle.audioIntervalId);
      handle.audioIntervalId = null;
    }
  };

  const reset = () => {
    state.frameData = defaultFrameData;
    state.audioChunkData = defaultAudioChunkData;
    state.emitAudio = false;
    state.frameIntervalMs = 33;
    state.audioIntervalMs = 120;
    state.createCount = 0;
    state.startCount = 0;
    state.stopCount = 0;
    state.lastVideoEnabled = true;
    state.lastAudioEnabled = true;
  };

  const configure = config => {
    if (config && Object.prototype.hasOwnProperty.call(config, 'frameData')) {
      state.frameData = config.frameData;
    }
    if (config && Object.prototype.hasOwnProperty.call(config, 'audioChunkData')) {
      state.audioChunkData = config.audioChunkData;
    }
    if (config && Object.prototype.hasOwnProperty.call(config, 'emitAudio')) {
      state.emitAudio = config.emitAudio;
    }
    if (config && Object.prototype.hasOwnProperty.call(config, 'frameIntervalMs')) {
      state.frameIntervalMs = config.frameIntervalMs;
    }
    if (config && Object.prototype.hasOwnProperty.call(config, 'audioIntervalMs')) {
      state.audioIntervalMs = config.audioIntervalMs;
    }
  };

  const snapshot = () => ({
    frameData: state.frameData,
    audioChunkData: state.audioChunkData,
    emitAudio: state.emitAudio,
    frameIntervalMs: state.frameIntervalMs,
    audioIntervalMs: state.audioIntervalMs,
    createCount: state.createCount,
    startCount: state.startCount,
    stopCount: state.stopCount,
    lastVideoEnabled: state.lastVideoEnabled,
    lastAudioEnabled: state.lastAudioEnabled,
  });

  globalThis.__RESYNC_VIDEO_CHAT_MEDIA_CAPTURE_OVERRIDE = {
    create() {
      state.createCount += 1;
      state.lastVideoEnabled = true;
      state.lastAudioEnabled = true;
      return Promise.resolve({
        frameIntervalId: null,
        audioIntervalId: null,
        videoEnabled: true,
        audioEnabled: true,
      });
    },
    attachVideoElement(_handle, _videoEl) {},
    startCapture(handle, callbacks) {
      state.startCount += 1;
      clearHandleIntervals(handle);
      handle.frameIntervalId = globalThis.setInterval(() => {
        if (handle.videoEnabled) {
          callbacks.onFrame(state.frameData);
        }
      }, state.frameIntervalMs);

      if (state.emitAudio) {
        handle.audioIntervalId = globalThis.setInterval(() => {
          if (handle.audioEnabled) {
            callbacks.onAudioChunk(state.audioChunkData);
          }
        }, state.audioIntervalMs);
      }
    },
    stopCapture(handle) {
      state.stopCount += 1;
      clearHandleIntervals(handle);
    },
    setVideoEnabled(handle, enabled) {
      handle.videoEnabled = enabled;
      state.lastVideoEnabled = enabled;
    },
    setAudioEnabled(handle, enabled) {
      handle.audioEnabled = enabled;
      state.lastAudioEnabled = enabled;
    },
  };

  globalThis.__RESYNC_VIDEO_CHAT_MEDIA_CAPTURE_MOCK = {
    reset,
    configure,
    snapshot,
  };
})();
|mock};

let install = () => {
  reset();
  setOverride(
    globalThis,
    makeImplementation(
      ~create,
      ~attachVideoElement,
      ~startCapture,
      ~stopCapture,
      ~setVideoEnabled,
      ~setAudioEnabled,
      (),
    ),
  );
  setController(globalThis, makeController(~reset, ~configure=configureFromJs, ~snapshot, ())); 
};

let () = install();
