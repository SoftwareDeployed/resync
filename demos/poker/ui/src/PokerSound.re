type cue =
  | Shuffle
  | Deal
  | Fold
  | Chips
  | Win;

let cueName = cue =>
  switch (cue) {
  | Shuffle => "shuffle"
  | Deal => "deal"
  | Fold => "fold"
  | Chips => "chips"
  | Win => "win"
  };

[@melange_only]
let sound = [%raw "(function() {\n  let enabled = false;\n  const audioByCue = Object.create(null);\n  const filesByCue = {\n    shuffle: 'card-shuffle.ogg',\n    deal: 'card-slide-6.ogg',\n    fold: 'card-slide-1.ogg',\n    chips: 'chips-collide-1.ogg',\n    win: 'chips-collide-2.ogg'\n  };\n  return {\n    setEnabled: function(nextEnabled) {\n      enabled = nextEnabled;\n      if (!enabled) {\n        Object.keys(audioByCue).forEach(function(key) {\n          const audio = audioByCue[key];\n          try {\n            audio.pause();\n            audio.currentTime = 0;\n          } catch (_) {}\n        });\n      }\n    },\n    playNamed: function(cue) {\n      if (!enabled) return;\n      try {\n        let audio = audioByCue[cue];\n        if (!audio) {\n          audio = new Audio('/static/sounds/' + filesByCue[cue]);\n          audio.preload = 'auto';\n          audioByCue[cue] = audio;\n        }\n        try {\n          audio.pause();\n          audio.currentTime = 0;\n        } catch (_) {}\n        try {\n          const playback = audio.play();\n          if (playback && typeof playback.catch === 'function') {\n            playback.catch(function() {});\n          }\n        } catch (_) {}\n      } catch (_) {}\n    }\n  };\n})()"];

[@melange_only]
let setEnabled : bool => unit = nextEnabled => sound##setEnabled(nextEnabled);

[@native_only]
let setEnabled = _enabled => ();

[@melange_only]
let playNamed : string => unit = cue => sound##playNamed(cue);

[@native_only]
let playNamed = _cue => ();

let play = cue => playNamed(cueName(cue));
