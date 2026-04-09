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

[@platform native]
let mediaDevices = (_navigator: navigator) => Obj.magic();

[@platform native]
let getUserMedia = (_mediaDevices: mediaDevices, _constraints: Js.t({. video: bool, audio: bool})) =>
  Js.Promise.resolve(Obj.magic());

[@platform native]
let toDataURL = (_canvas: canvasElement, _mimeType: string, _quality: float) => "";

[@platform native]
let drawVideoImage = (
  _ctx: canvasRenderingContext2d,
  _video: videoElement,
  _x: int,
  _y: int,
  _width: int,
  _height: int,
) => ();

[@platform native]
let drawImage = (_ctx: canvasRenderingContext2d, _image: image, _x: int, _y: int) => ();

[@platform native]
let makeAudio = (_src: string) => Obj.magic();

[@platform native]
let playAudio = (_audio: audio) => Js.Promise.resolve();

[@platform native]
let setAutoplay = (_video: videoElement, _autoplay: bool) => ();

[@platform native]
let setMuted = (_video: videoElement, _muted: bool) => ();

[@platform native]
let setPlaysInline = (_video: videoElement, _playsInline: bool) => ();

[@platform native]
let setVideoSrcObject = (_video: videoElement, _stream: mediaStream) => ();

[@platform native]
let playVideo = (_video: videoElement) => Js.Promise.resolve();

[@platform native]
let setElementSrcObject = (_element: 'element, _stream: mediaStream) => ();

[@platform native]
let setElementMuted = (_element: 'element, _muted: bool) => ();

[@platform native]
let playElement = (_element: 'element) => Js.Promise.resolve();

[@platform native]
let getTracks = (_stream: mediaStream) => [||];

[@platform native]
let getVideoTracks = (_stream: mediaStream) => [||];

[@platform native]
let getAudioTracks = (_stream: mediaStream) => [||];

[@platform native]
let setEnabled = (_track: mediaTrack, _enabled: bool) => ();

[@platform native]
let stopTrack = (_track: mediaTrack) => ();

[@platform native]
let makeAudioContext = (_config: Js.t({. sampleRate: int})) => Obj.magic();

[@platform native]
let makeWebkitAudioContext = (_config: Js.t({. sampleRate: int})) => Obj.magic();

[@platform native]
let createMediaStreamSource = (_ctx: audioContext, _stream: mediaStream) => Obj.magic();

[@platform native]
let createScriptProcessor = (_ctx: audioContext, _bufferSize: int, _inputChannels: int, _outputChannels: int) =>
  Obj.magic();

[@platform native]
let createGain = (_ctx: audioContext) => Obj.magic();

[@platform native]
let gainParam = (_gain: gainNode) => Obj.magic();

[@platform native]
let setAudioParamValue = (_param: audioParam, _value: float) => ();

[@platform native]
let connect = (_source, _destination) => ();

[@platform native]
let disconnect = _node => ();

[@platform native]
let audioContextDestination = (_ctx: audioContext) => Obj.magic();

[@platform native]
let audioContextState = (_ctx: audioContext) => "closed";

[@platform native]
let resumeAudioContext = (_ctx: audioContext) => Js.Promise.resolve();

[@platform native]
let closeAudioContext = (_ctx: audioContext) => Js.Promise.resolve();

[@platform native]
let audioContextSampleRate = (_ctx: audioContext) => 0;

[@platform native]
let setOnAudioProcess = (_processor: scriptProcessorNode, _handler: audioProcessingEvent => unit) => ();

[@platform native]
let inputBuffer = (_event: audioProcessingEvent) => Obj.magic();

[@platform native]
let getChannelData = (_buffer: audioBuffer, _channel: int) => Obj.magic();

[@platform native]
let float32Length = (_buffer: float32Array) => 0;

[@platform native]
let float32At = (_buffer: float32Array, _index: int) => 0.0;

[@platform native]
let makeArrayBuffer = (_byteLength: int) => Obj.magic();

[@platform native]
let makeDataView = (_buffer: arrayBuffer) => Obj.magic();

[@platform native]
let setUint8 = (_view: dataView, _offset: int, _value: int) => ();

[@platform native]
let setUint16 = (_view: dataView, _offset: int, _value: int, _littleEndian: bool) => ();

[@platform native]
let setUint32 = (_view: dataView, _offset: int, _value: int, _littleEndian: bool) => ();

[@platform native]
let setInt16 = (_view: dataView, _offset: int, _value: int, _littleEndian: bool) => ();

[@platform native]
let makeUint8Array = (_buffer: arrayBuffer) => Obj.magic();

[@platform native]
let uint8Length = (_buffer: uint8Array) => 0;

[@platform native]
let uint8At = (_buffer: uint8Array, _index: int) => 0;

[@platform native]
let fromCharCode = (_charCode: int) => "";

[@platform native]
let setInterval = (_callback: unit => unit, _delay: int) => 0;

[@platform native]
let clearInterval = (_intervalId: int) => ();

[@platform js]
[@mel.get] external mediaDevices: navigator => mediaDevices = "mediaDevices";
[@platform js]
[@mel.send]
external getUserMedia: (mediaDevices, Js.t({. video: bool, audio: bool})) => Js.Promise.t(mediaStream) = "getUserMedia";
[@platform js]
[@mel.send]
external toDataURL: (canvasElement, string, float) => string = "toDataURL";
[@platform js]
[@mel.send]
external drawVideoImage: (canvasRenderingContext2d, videoElement, int, int, int, int) => unit = "drawImage";
[@platform js]
[@mel.send]
external drawImage: (canvasRenderingContext2d, image, int, int) => unit = "drawImage";
[@platform js]
[@mel.new] external makeAudio: string => audio = "Audio";
[@platform js]
[@mel.send] external playAudio: audio => Js.Promise.t(unit) = "play";

[@platform js]
[@mel.set] external setAutoplay: (videoElement, bool) => unit = "autoplay";
[@platform js]
[@mel.set] external setMuted: (videoElement, bool) => unit = "muted";
[@platform js]
[@mel.set] external setPlaysInline: (videoElement, bool) => unit = "playsInline";
[@platform js]
[@mel.set] external setVideoSrcObject: (videoElement, mediaStream) => unit = "srcObject";
[@platform js]
[@mel.send] external playVideo: videoElement => Js.Promise.t(unit) = "play";

[@platform js]
[@mel.set] external setElementSrcObject: ('element, mediaStream) => unit = "srcObject";
[@platform js]
[@mel.set] external setElementMuted: ('element, bool) => unit = "muted";
[@platform js]
[@mel.send] external playElement: 'element => Js.Promise.t(unit) = "play";

[@platform js]
[@mel.send] external getTracks: mediaStream => array(mediaTrack) = "getTracks";
[@platform js]
[@mel.send] external getVideoTracks: mediaStream => array(mediaTrack) = "getVideoTracks";
[@platform js]
[@mel.send] external getAudioTracks: mediaStream => array(mediaTrack) = "getAudioTracks";
[@platform js]
[@mel.set] external setEnabled: (mediaTrack, bool) => unit = "enabled";
[@platform js]
[@mel.send] external stopTrack: mediaTrack => unit = "stop";

[@platform js]
[@mel.new]
external makeAudioContext: Js.t({. sampleRate: int}) => audioContext = "AudioContext";
[@platform js]
[@mel.scope "window"] [@mel.new]
external makeWebkitAudioContext: Js.t({. sampleRate: int}) => audioContext = "webkitAudioContext";

[@platform js]
[@mel.send]
external createMediaStreamSource: (audioContext, mediaStream) => audioSourceNode = "createMediaStreamSource";
[@platform js]
[@mel.send]
external createScriptProcessor: (audioContext, int, int, int) => scriptProcessorNode = "createScriptProcessor";
[@platform js]
[@mel.send] external createGain: audioContext => gainNode = "createGain";
[@platform js]
[@mel.get] external gainParam: gainNode => audioParam = "gain";
[@platform js]
[@mel.set] external setAudioParamValue: (audioParam, float) => unit = "value";
[@platform js]
[@mel.send] external connect: ('a, 'b) => unit = "connect";
[@platform js]
[@mel.send] external disconnect: 'a => unit = "disconnect";
[@platform js]
[@mel.get] external audioContextDestination: audioContext => audioDestinationNode = "destination";
[@platform js]
[@mel.get] external audioContextState: audioContext => string = "state";
[@platform js]
[@mel.send] external resumeAudioContext: audioContext => Js.Promise.t(unit) = "resume";
[@platform js]
[@mel.send] external closeAudioContext: audioContext => Js.Promise.t(unit) = "close";
[@platform js]
[@mel.get] external audioContextSampleRate: audioContext => int = "sampleRate";

[@platform js]
[@mel.set]
external setOnAudioProcess: (scriptProcessorNode, audioProcessingEvent => unit) => unit = "onaudioprocess";
[@platform js]
[@mel.get] external inputBuffer: audioProcessingEvent => audioBuffer = "inputBuffer";
[@platform js]
[@mel.send] external getChannelData: (audioBuffer, int) => float32Array = "getChannelData";
[@platform js]
[@mel.get] external float32Length: float32Array => int = "length";
[@platform js]
[@mel.get_index] external float32At: (float32Array, int) => float = "";

[@platform js]
[@mel.new] external makeArrayBuffer: int => arrayBuffer = "ArrayBuffer";
[@platform js]
[@mel.new] external makeDataView: arrayBuffer => dataView = "DataView";
[@platform js]
[@mel.send] external setUint8: (dataView, int, int) => unit = "setUint8";
[@platform js]
[@mel.send] external setUint16: (dataView, int, int, bool) => unit = "setUint16";
[@platform js]
[@mel.send] external setUint32: (dataView, int, int, bool) => unit = "setUint32";
[@platform js]
[@mel.send] external setInt16: (dataView, int, int, bool) => unit = "setInt16";
[@platform js]
[@mel.new] external makeUint8Array: arrayBuffer => uint8Array = "Uint8Array";
[@platform js]
[@mel.get] external uint8Length: uint8Array => int = "length";
[@platform js]
[@mel.get_index] external uint8At: (uint8Array, int) => int = "";
[@platform js]
[@mel.scope "String"] external fromCharCode: int => string = "fromCharCode";

[@platform js]
external setInterval: (unit => unit, int) => int = "setInterval";
[@platform js]
external clearInterval: int => unit = "clearInterval";
