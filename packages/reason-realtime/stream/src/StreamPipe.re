type unsubscribe = unit => unit;

type t('a) = {
  subscribe: ('a => unit) => unsubscribe,
};

let make = (~subscribe: ('a => unit) => unsubscribe) : t('a) => {
  {subscribe: subscribe};
};

let map = (pipe: t('a), f: 'a => 'b) : t('b) => {
  make(~subscribe=next => {
    pipe.subscribe(a => next(f(a)));
  });
};

let filterMap = (pipe: t('a), f: 'a => option('b)) : t('b) => {
  make(~subscribe=next => {
    pipe.subscribe(a => {
      switch (f(a)) {
      | Some(b) => next(b)
      | None => ()
      };
    });
  });
};

let tap = (pipe: t('a), sideEffect: 'a => unit) : t('a) => {
  make(~subscribe=next => {
    pipe.subscribe(a => {
      sideEffect(a);
      next(a);
    });
  });
};

let subscribe = (pipe: t('a), next: 'a => unit) : unsubscribe => {
  pipe.subscribe(next);
};

let toPromise = (pipe: t('a)) : Js.Promise.t(unit) => {
  Js.Promise.make((~resolve, ~reject as _) => {
    let resolveUnit = v => resolve(. v);
    let _unsubscribe = pipe.subscribe(_a => {
      resolveUnit(());
    });
    ();
  });
};
