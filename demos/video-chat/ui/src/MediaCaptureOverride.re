type implementation;
type handle;
type startCaptureCallbacks;

[@mel.obj]
external makeStartCaptureCallbacks:
  (~onFrame: string => unit, ~onAudioChunk: string => unit, unit) =>
  startCaptureCallbacks = "";

[@platform js]
external globalThis: 'a = "globalThis";

[@platform js] [@mel.get]
external getImplementation:
  'a => Js.Nullable.t(implementation) = "__RESYNC_VIDEO_CHAT_MEDIA_CAPTURE_OVERRIDE";

[@platform js] [@mel.send]
external create: implementation => Js.Promise.t(handle) = "create";

[@platform js] [@mel.send]
external attachVideoElement: (implementation, handle, 'element) => unit = "attachVideoElement";

[@platform js] [@mel.send]
external startCapture: (implementation, handle, startCaptureCallbacks) => unit = "startCapture";

[@platform js] [@mel.send]
external stopCapture: (implementation, handle) => unit = "stopCapture";

[@platform js] [@mel.send]
external setVideoEnabled: (implementation, handle, bool) => unit = "setVideoEnabled";

[@platform js] [@mel.send]
external setAudioEnabled: (implementation, handle, bool) => unit = "setAudioEnabled";

[@platform js]
let current = () => Js.Nullable.toOption(getImplementation(globalThis));

[@platform native]
let current = () => None;
