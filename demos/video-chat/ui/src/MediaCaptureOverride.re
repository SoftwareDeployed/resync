type implementation;
type handle;
type startCaptureCallbacks;

[@mel.obj]
external makeStartCaptureCallbacks:
  (~onFrame: string => unit, ~onAudioChunk: string => unit, unit) =>
  startCaptureCallbacks = "";

[@melange_only]
external globalThis: 'a = "globalThis";

[@melange_only] [@mel.get]
external getImplementation:
  'a => Js.Nullable.t(implementation) = "__RESYNC_VIDEO_CHAT_MEDIA_CAPTURE_OVERRIDE";

[@melange_only] [@mel.send]
external create: implementation => Js.Promise.t(handle) = "create";

[@melange_only] [@mel.send]
external attachVideoElement: (implementation, handle, 'element) => unit = "attachVideoElement";

[@melange_only] [@mel.send]
external startCapture: (implementation, handle, startCaptureCallbacks) => unit = "startCapture";

[@melange_only] [@mel.send]
external stopCapture: (implementation, handle) => unit = "stopCapture";

[@melange_only] [@mel.send]
external setVideoEnabled: (implementation, handle, bool) => unit = "setVideoEnabled";

[@melange_only] [@mel.send]
external setAudioEnabled: (implementation, handle, bool) => unit = "setAudioEnabled";

[@melange_only]
let current = () => Js.Nullable.toOption(getImplementation(globalThis));

[@native_only]
let current = () => None;
