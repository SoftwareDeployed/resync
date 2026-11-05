type t('a) = {
  value: 'a,
  get: unit => 'a,
  set: 'a => unit,
  update: ('a => 'a) => unit,
};

let make = (initial: 'a): t('a) =>
  switch%platform () {
  | Melange =>
    let (signal, setSignal) = Tilia.Core.signal(initial);
    {
      value: signal->Tilia.Core.lift,
      get: () => signal->Tilia.Core.value,
      set: setSignal,
      update: reducer => signal->Tilia.Core.lift |> reducer |> setSignal,
    }
  | Native =>
    let currentValue = ref(initial);
    {
      value: initial,
      get: () => currentValue.contents,
      set: next => currentValue := next,
      update: reducer => currentValue := reducer(currentValue.contents),
    }
  };
