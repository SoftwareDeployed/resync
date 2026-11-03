type mediaTrack;
type mediaStream;
type mediaDevices;
type navigator;
type canvasElement;
type canvasRenderingContext2d;
type videoElement;
type image;
type audio;
type audioContext;
type audioSourceNode;
type scriptProcessorNode;
type gainNode;
type audioParam;
type audioDestinationNode;
type audioProcessingEvent;
type audioBuffer;
type float32Array;
type arrayBuffer;
type dataView;
type uint8Array;
type scrollElement;

[@native_only]
let unavailable = () =>
  raise(Failure("MediaBindings raw browser bindings are only available in the browser"));

[@native_only]
let canvasRenderingContext2dOfDom = (_ctx: 'ctx) => unavailable();

[@native_only]
let canvasElementOfDom = (_element: 'element) => unavailable();

[@native_only]
let videoElementOfDom = (_element: 'element) => unavailable();

[@native_only]
let imageOfHtmlImage = (_image: 'image) => unavailable();

[@native_only]
let navigatorOfDom = (_navigator: 'navigator) => unavailable();

[@native_only]
let scrollElementOfDom = (_element: 'element) => unavailable();

[@native_only]
let scrollHeight = (_element: scrollElement) => 0.0;

[@native_only]
let setScrollTop = (_element: scrollElement, _value: float) => ();

[@native_only]
let mediaDevices = (_navigator: navigator) => unavailable();

[@native_only]
let getUserMedia = (_mediaDevices: mediaDevices, _constraints: Js.t({. video: bool, audio: bool})) =>
  Js.Promise.reject(Failure("getUserMedia is only available in the browser"));

[@native_only]
let toDataURL = (_canvas: canvasElement, _mimeType: string, _quality: float) => "";

[@native_only]
let drawVideoImage = (
  _ctx: canvasRenderingContext2d,
  _video: videoElement,
  _x: int,
  _y: int,
  _width: int,
  _height: int,
) => ();

[@native_only]
let drawImage = (_ctx: canvasRenderingContext2d, _image: image, _x: int, _y: int) => ();

[@native_only]
let makeAudio = (_src: string) => unavailable();

[@native_only]
let playAudio = (_audio: audio) => Js.Promise.resolve();

[@native_only]
let setAutoplay = (_video: videoElement, _autoplay: bool) => ();

[@native_only]
let setMuted = (_video: videoElement, _muted: bool) => ();

[@native_only]
let setPlaysInline = (_video: videoElement, _playsInline: bool) => ();

[@native_only]
let setVideoSrcObject = (_video: videoElement, _stream: mediaStream) => ();

[@native_only]
let playVideo = (_video: videoElement) => Js.Promise.resolve();

[@native_only]
let setElementSrcObject = (_element: 'element, _stream: mediaStream) => ();

[@native_only]
let setElementMuted = (_element: 'element, _muted: bool) => ();

[@native_only]
let playElement = (_element: 'element) => Js.Promise.resolve();

[@native_only]
let getTracks = (_stream: mediaStream) => [||];

[@native_only]
let getVideoTracks = (_stream: mediaStream) => [||];

[@native_only]
let getAudioTracks = (_stream: mediaStream) => [||];

[@native_only]
let setEnabled = (_track: mediaTrack, _enabled: bool) => ();

[@native_only]
let stopTrack = (_track: mediaTrack) => ();

[@native_only]
let makeAudioContext = (_config: Js.t({. sampleRate: int})) => unavailable();

[@native_only]
let makeWebkitAudioContext = (_config: Js.t({. sampleRate: int})) => unavailable();

[@native_only]
let createMediaStreamSource = (_ctx: audioContext, _stream: mediaStream) => unavailable();

[@native_only]
let createScriptProcessor = (_ctx: audioContext, _bufferSize: int, _inputChannels: int, _outputChannels: int) =>
  unavailable();

[@native_only]
let createGain = (_ctx: audioContext) => unavailable();

[@native_only]
let gainParam = (_gain: gainNode) => unavailable();

[@native_only]
let setAudioParamValue = (_param: audioParam, _value: float) => ();

[@native_only]
let connect = (_source, _destination) => ();

[@native_only]
let disconnect = _node => ();

[@native_only]
let audioContextDestination = (_ctx: audioContext) => unavailable();

[@native_only]
let audioContextState = (_ctx: audioContext) => "closed";

[@native_only]
let resumeAudioContext = (_ctx: audioContext) => Js.Promise.resolve();

[@native_only]
let closeAudioContext = (_ctx: audioContext) => Js.Promise.resolve();

[@native_only]
let audioContextSampleRate = (_ctx: audioContext) => 0;

[@native_only]
let setOnAudioProcess = (_processor: scriptProcessorNode, _handler: audioProcessingEvent => unit) => ();

[@native_only]
let inputBuffer = (_event: audioProcessingEvent) => unavailable();

[@native_only]
let getChannelData = (_buffer: audioBuffer, _channel: int) => unavailable();

[@native_only]
let float32Length = (_buffer: float32Array) => 0;

[@native_only]
let float32At = (_buffer: float32Array, _index: int) => 0.0;

[@native_only]
let makeArrayBuffer = (_byteLength: int) => unavailable();

[@native_only]
let makeDataView = (_buffer: arrayBuffer) => unavailable();

[@native_only]
let setUint8 = (_view: dataView, _offset: int, _value: int) => ();

[@native_only]
let setUint16 = (_view: dataView, _offset: int, _value: int, _littleEndian: bool) => ();

[@native_only]
let setUint32 = (_view: dataView, _offset: int, _value: int, _littleEndian: bool) => ();

[@native_only]
let setInt16 = (_view: dataView, _offset: int, _value: int, _littleEndian: bool) => ();

[@native_only]
let makeUint8Array = (_buffer: arrayBuffer) => unavailable();

[@native_only]
let uint8Length = (_buffer: uint8Array) => 0;

[@native_only]
let uint8At = (_buffer: uint8Array, _index: int) => 0;

[@native_only]
let fromCharCode = (_charCode: int) => "";

[@native_only]
let setInterval = (_callback: unit => unit, _delay: int) => 0;

[@native_only]
let clearInterval = (_intervalId: int) => ();

[@melange_only]
external canvasRenderingContext2dOfDom: 'ctx => canvasRenderingContext2d = "%identity";
[@melange_only]
external canvasElementOfDom: 'element => canvasElement = "%identity";
[@melange_only]
external videoElementOfDom: 'element => videoElement = "%identity";
[@melange_only]
external imageOfHtmlImage: 'image => image = "%identity";
[@melange_only]
external navigatorOfDom: 'navigator => navigator = "%identity";
[@melange_only]
external scrollElementOfDom: 'element => scrollElement = "%identity";
[@melange_only]
[@mel.get] external scrollHeight: scrollElement => float = "scrollHeight";
[@melange_only]
[@mel.set] external setScrollTop: (scrollElement, float) => unit = "scrollTop";
[@melange_only]
[@mel.get] external mediaDevices: navigator => mediaDevices = "mediaDevices";
[@melange_only]
[@mel.send]
external getUserMedia: (mediaDevices, Js.t({. video: bool, audio: bool})) => Js.Promise.t(mediaStream) = "getUserMedia";
[@melange_only]
[@mel.send]
external toDataURL: (canvasElement, string, float) => string = "toDataURL";
[@melange_only]
[@mel.send]
external drawVideoImage: (canvasRenderingContext2d, videoElement, int, int, int, int) => unit = "drawImage";
[@melange_only]
[@mel.send]
external drawImage: (canvasRenderingContext2d, image, int, int) => unit = "drawImage";
[@melange_only]
[@mel.new] external makeAudio: string => audio = "Audio";
[@melange_only]
[@mel.send] external playAudio: audio => Js.Promise.t(unit) = "play";

[@melange_only]
[@mel.set] external setAutoplay: (videoElement, bool) => unit = "autoplay";
[@melange_only]
[@mel.set] external setMuted: (videoElement, bool) => unit = "muted";
[@melange_only]
[@mel.set] external setPlaysInline: (videoElement, bool) => unit = "playsInline";
[@melange_only]
[@mel.set] external setVideoSrcObject: (videoElement, mediaStream) => unit = "srcObject";
[@melange_only]
[@mel.send] external playVideo: videoElement => Js.Promise.t(unit) = "play";

[@melange_only]
[@mel.set] external setElementSrcObject: ('element, mediaStream) => unit = "srcObject";
[@melange_only]
[@mel.set] external setElementMuted: ('element, bool) => unit = "muted";
[@melange_only]
[@mel.send] external playElement: 'element => Js.Promise.t(unit) = "play";

[@melange_only]
[@mel.send] external getTracks: mediaStream => array(mediaTrack) = "getTracks";
[@melange_only]
[@mel.send] external getVideoTracks: mediaStream => array(mediaTrack) = "getVideoTracks";
[@melange_only]
[@mel.send] external getAudioTracks: mediaStream => array(mediaTrack) = "getAudioTracks";
[@melange_only]
[@mel.set] external setEnabled: (mediaTrack, bool) => unit = "enabled";
[@melange_only]
[@mel.send] external stopTrack: mediaTrack => unit = "stop";

[@melange_only]
[@mel.new]
external makeAudioContext: Js.t({. sampleRate: int}) => audioContext = "AudioContext";
[@melange_only]
[@mel.scope "window"] [@mel.new]
external makeWebkitAudioContext: Js.t({. sampleRate: int}) => audioContext = "webkitAudioContext";

[@melange_only]
[@mel.send]
external createMediaStreamSource: (audioContext, mediaStream) => audioSourceNode = "createMediaStreamSource";
[@melange_only]
[@mel.send]
external createScriptProcessor: (audioContext, int, int, int) => scriptProcessorNode = "createScriptProcessor";
[@melange_only]
[@mel.send] external createGain: audioContext => gainNode = "createGain";
[@melange_only]
[@mel.get] external gainParam: gainNode => audioParam = "gain";
[@melange_only]
[@mel.set] external setAudioParamValue: (audioParam, float) => unit = "value";
[@melange_only]
[@mel.send] external connect: ('a, 'b) => unit = "connect";
[@melange_only]
[@mel.send] external disconnect: 'a => unit = "disconnect";
[@melange_only]
[@mel.get] external audioContextDestination: audioContext => audioDestinationNode = "destination";
[@melange_only]
[@mel.get] external audioContextState: audioContext => string = "state";
[@melange_only]
[@mel.send] external resumeAudioContext: audioContext => Js.Promise.t(unit) = "resume";
[@melange_only]
[@mel.send] external closeAudioContext: audioContext => Js.Promise.t(unit) = "close";
[@melange_only]
[@mel.get] external audioContextSampleRate: audioContext => int = "sampleRate";

[@melange_only]
[@mel.set]
external setOnAudioProcess: (scriptProcessorNode, audioProcessingEvent => unit) => unit = "onaudioprocess";
[@melange_only]
[@mel.get] external inputBuffer: audioProcessingEvent => audioBuffer = "inputBuffer";
[@melange_only]
[@mel.send] external getChannelData: (audioBuffer, int) => float32Array = "getChannelData";
[@melange_only]
[@mel.get] external float32Length: float32Array => int = "length";
[@melange_only]
[@mel.get_index] external float32At: (float32Array, int) => float = "";

[@melange_only]
[@mel.new] external makeArrayBuffer: int => arrayBuffer = "ArrayBuffer";
[@melange_only]
[@mel.new] external makeDataView: arrayBuffer => dataView = "DataView";
[@melange_only]
[@mel.send] external setUint8: (dataView, int, int) => unit = "setUint8";
[@melange_only]
[@mel.send] external setUint16: (dataView, int, int, bool) => unit = "setUint16";
[@melange_only]
[@mel.send] external setUint32: (dataView, int, int, bool) => unit = "setUint32";
[@melange_only]
[@mel.send] external setInt16: (dataView, int, int, bool) => unit = "setInt16";
[@melange_only]
[@mel.new] external makeUint8Array: arrayBuffer => uint8Array = "Uint8Array";
[@melange_only]
[@mel.get] external uint8Length: uint8Array => int = "length";
[@melange_only]
[@mel.get_index] external uint8At: (uint8Array, int) => int = "";
[@melange_only]
[@mel.scope "String"] external fromCharCode: int => string = "fromCharCode";

[@melange_only]
external setInterval: (unit => unit, int) => int = "setInterval";
[@melange_only]
external clearInterval: int => unit = "clearInterval";
