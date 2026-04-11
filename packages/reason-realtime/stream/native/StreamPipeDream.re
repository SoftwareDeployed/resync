open Lwt.Syntax;

type t('a) = {
  pipe: StreamPipe.t('a),
  finished: Lwt.t(unit),
};

let fromStream = (stream: Dream.stream) : t(string) => {
  let taskPair = Lwt.task();
  let finished = fst(taskPair);
  let resolve_finished = snd(taskPair);
  let pipe = StreamPipe.make(~subscribe=(next => {
    let cancel = ref(false);
    let rec loop = () => {
      if (cancel^) {
        Lwt.return_unit;
      } else {
        let* chunk = Dream.read(stream);
        switch (chunk) {
        | Some(data) =>
          next(data);
          loop();
        | None =>
          Lwt.wakeup_later(resolve_finished, ());
          Lwt.return_unit
        };
      };
    };
    Lwt.async(loop);
    () => cancel := true;
  }));
  {pipe, finished};
};

let broadcast = (dreamPipe: t('a), ~send: 'a => unit) : Lwt.t(unit) => {
  let _unsubscribe = StreamPipe.subscribe(dreamPipe.pipe, value => send(value));
  dreamPipe.finished;
};

let fromLwtStream = (stream: Lwt_stream.t(string)) : t(string) => {
  let taskPair = Lwt.task();
  let finished = fst(taskPair);
  let resolve_finished = snd(taskPair);
  let pipe = StreamPipe.make(~subscribe=(next => {
    let cancel = ref(false);
    let rec loop = () => {
      if (cancel^) {
        Lwt.return_unit;
      } else {
        let* chunk = Lwt_stream.get(stream);
        switch (chunk) {
        | Some(data) =>
          next(data);
          loop();
        | None =>
          Lwt.wakeup_later(resolve_finished, ());
          Lwt.return_unit
        };
      };
    };
    Lwt.async(loop);
    () => cancel := true;
  }));
  {pipe, finished};
};

let broadcast_with_lwt = (dreamPipe: t('a), ~send: 'a => Lwt.t(unit)) : Lwt.t(unit) => {
  let _unsubscribe = StreamPipe.subscribe(dreamPipe.pipe, value => {
    Lwt.ignore_result(send(value));
  });
  dreamPipe.finished;
};
