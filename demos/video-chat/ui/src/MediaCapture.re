type handle;
type mediaTrack;

type t = handle;

let width = 640;
let height = 480;

[@platform js]
let create: unit => Js.Promise.t(handle) = [%raw
  {|
  function() {
    return navigator.mediaDevices.getUserMedia({ video: true, audio: true }).then(function(stream) {
      const canvas = document.createElement("canvas");
      canvas.width = 640;
      canvas.height = 480;
      const ctx = canvas.getContext("2d");
      const captureVideo = document.createElement("video");
      captureVideo.autoplay = true;
      captureVideo.muted = true;
      captureVideo.playsInline = true;
      captureVideo.srcObject = stream;
      const playPromise = captureVideo.play();
      if (playPromise && playPromise.catch) {
        playPromise.catch(() => undefined);
      }

      let audioContext = null;
      let audioSource = null;
      let audioProcessor = null;
      let audioGain = null;
      try {
        const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
        if (AudioContextCtor && stream.getAudioTracks().length > 0) {
          audioContext = new AudioContextCtor({ sampleRate: 16000 });
          audioSource = audioContext.createMediaStreamSource(stream);
          audioProcessor = audioContext.createScriptProcessor(4096, 1, 1);
          audioGain = audioContext.createGain();
          audioGain.gain.value = 0;
          audioSource.connect(audioProcessor);
          audioProcessor.connect(audioGain);
          audioGain.connect(audioContext.destination);
          if (audioContext.state === "suspended" && audioContext.resume) {
            audioContext.resume().catch(() => undefined);
          }
        }
      } catch (_error) {
        audioContext = null;
        audioSource = null;
        audioProcessor = null;
        audioGain = null;
      }

      return {
        stream,
        canvas,
        ctx,
        captureVideo,
        captureInterval: null,
        audioContext,
        audioSource,
        audioProcessor,
        audioGain,
        audioEnabled: true,
      };
    });
  }
  |}
];

[@platform native]
let create = () => Js.Promise.resolve(Obj.magic());

[@platform js]
let attachVideoElementJs: (handle, Dom.element) => unit = [%raw
  {|
  function(handle, videoEl) {
    videoEl.srcObject = handle.stream;
    videoEl.muted = true;
    const playPromise = videoEl.play();
    if (playPromise && playPromise.catch) {
      playPromise.catch(() => undefined);
    }
  }
  |}
];

[@platform native]
let attachVideoElementJs = (_handle, _videoEl) => ();

let attachVideoElement = (capture, videoEl) =>
  attachVideoElementJs(capture, videoEl);

[@platform js]
let startCaptureJs: (handle, string => unit, string => unit) => unit = [%raw
  {|
  (function() {
    function writeAscii(view, offset, text) {
      for (let i = 0; i < text.length; i++) {
        view.setUint8(offset + i, text.charCodeAt(i));
      }
    }

    function wavDataUrlFromSamples(samples, sampleRate) {
      const buffer = new ArrayBuffer(44 + samples.length * 2);
      const view = new DataView(buffer);
      writeAscii(view, 0, "RIFF");
      view.setUint32(4, 36 + samples.length * 2, true);
      writeAscii(view, 8, "WAVE");
      writeAscii(view, 12, "fmt ");
      view.setUint32(16, 16, true);
      view.setUint16(20, 1, true);
      view.setUint16(22, 1, true);
      view.setUint32(24, sampleRate, true);
      view.setUint32(28, sampleRate * 2, true);
      view.setUint16(32, 2, true);
      view.setUint16(34, 16, true);
      writeAscii(view, 36, "data");
      view.setUint32(40, samples.length * 2, true);

      let peak = 0;
      for (let i = 0; i < samples.length; i++) {
        const value = Math.max(-1, Math.min(1, samples[i]));
        peak = Math.max(peak, Math.abs(value));
        view.setInt16(44 + i * 2, value < 0 ? value * 0x8000 : value * 0x7fff, true);
      }

      if (peak < 0.003) {
        return null;
      }

      const bytes = new Uint8Array(buffer);
      let binary = "";
      const chunkSize = 0x8000;
      for (let i = 0; i < bytes.length; i += chunkSize) {
        const chunk = bytes.subarray(i, i + chunkSize);
        binary += String.fromCharCode.apply(null, chunk);
      }
      return "data:audio/wav;base64," + btoa(binary);
    }

 return function(handle, onFrame, onAudioChunk) {
 handle.captureInterval = setInterval(function() {
 try {
 handle.ctx.drawImage(handle.captureVideo, 0, 0, 640, 480);
 onFrame(handle.canvas.toDataURL("image/jpeg", 0.7));
 } catch (_error) {
 }
 }, 33);

      if (handle.audioProcessor && handle.audioContext) {
        handle.audioProcessor.onaudioprocess = function(event) {
          if (!handle.audioEnabled) {
            return;
          }
          const input = event.inputBuffer.getChannelData(0);
          if (!input || input.length === 0) {
            return;
          }
          const wavDataUrl = wavDataUrlFromSamples(input, handle.audioContext.sampleRate || 16000);
          if (wavDataUrl) {
            onAudioChunk(wavDataUrl);
          }
        };
      }
    };
  })()
  |}
];

[@platform native]
let startCaptureJs = (_handle, _onFrame, _onAudioChunk) => ();

let startCapture = (capture, ~onFrame, ~onAudioChunk) =>
  startCaptureJs(capture, onFrame, onAudioChunk);

[@platform js]
let stopCaptureJs: handle => unit = [%raw
  {|
  function(handle) {
    if (handle.captureInterval != null) {
      clearInterval(handle.captureInterval);
      handle.captureInterval = null;
    }

    if (handle.audioProcessor) {
      try {
        handle.audioProcessor.disconnect();
      } catch (_error) {
      }
    }

    if (handle.audioSource) {
      try {
        handle.audioSource.disconnect();
      } catch (_error) {
      }
    }

    if (handle.audioGain) {
      try {
        handle.audioGain.disconnect();
      } catch (_error) {
      }
    }

    if (handle.audioContext && handle.audioContext.close) {
      handle.audioContext.close().catch(() => undefined);
    }

    const tracks = handle.stream.getTracks();
    tracks.forEach(function(track) {
      track.enabled = false;
      track.stop();
    });
  }
  |}
];

[@platform native]
let stopCaptureJs = _handle => ();

let stopCapture = capture => stopCaptureJs(capture);

[@platform js]
let getVideoTracks: handle => array(mediaTrack) = [%raw
  {|
  function(handle) {
    return handle.stream.getVideoTracks();
  }
  |}
];

[@platform native]
let getVideoTracks = _handle => [||];

[@platform js]
let getAudioTracks: handle => array(mediaTrack) = [%raw
  {|
  function(handle) {
    return handle.stream.getAudioTracks();
  }
  |}
];

[@platform native]
let getAudioTracks = _handle => [||];
[@mel.set] external setEnabled: (mediaTrack, bool) => unit = "enabled";

[@platform js]
let setAudioCaptureEnabled: (handle, bool) => unit = [%raw
  {|
  function(handle, enabled) {
    handle.audioEnabled = enabled;
  }
  |}
];

[@platform native]
let setAudioCaptureEnabled = (_handle, _enabled) => ();

let setVideoEnabled = (capture, enabled) => {
  let tracks = getVideoTracks(capture);
  Js.Array.forEach(~f=track => setEnabled(track, enabled), tracks);
};

let setAudioEnabled = (capture, enabled) => {
  setAudioCaptureEnabled(capture, enabled);
  let tracks = getAudioTracks(capture);
  Js.Array.forEach(~f=track => setEnabled(track, enabled), tracks);
};
