type actions('a) = {
  get: unit => 'a,
  set: 'a => unit,
  update: ('a => 'a) => unit,
};

type t('a) = {
  value: 'a,
  get: unit => 'a,
  set: 'a => unit,
  update: ('a => 'a) => unit,
};

let make = (~afterSet=((_next: 'a) => ()), ~mount: option(actions('a) => unit)=?, initial: 'a) : t('a) => {
  let currentValue = ref(initial);
  let setValueRef = ref((next: 'a) => {
    currentValue := next;
    afterSet(next);
  });

  let actions = {
    get: () => currentValue.contents,
    set: next => setValueRef.contents(next),
    update: reducer => setValueRef.contents(reducer(currentValue.contents)),
  };

  let value =
    switch%platform (Runtime.platform) {
    | Client =>
      Tilia.Core.source(. initial, (. _value, setValue) => {
        setValueRef := next => {
          currentValue := next;
          setValue(next);
          afterSet(next);
        };

        switch (mount) {
        | Some(runMount) => runMount(actions)
        | None => ()
        };
      })
    | Server => initial
    };

  {
    value,
    get: actions.get,
    set: actions.set,
    update: actions.update,
  };
};
