type user = {id: string, role: string};

type auth_state =
  | Anonymous
  | Authenticated(user);

type t = {signal: StoreSignal.t(auth_state)};

let make = (~initial=Anonymous, ()) => {signal: StoreSignal.make(initial)};

let state = t => t.signal.get();
let set_state = (t, next) => t.signal.set(next);
let update = (t, reducer) => t.signal.update(reducer);

let user = t =>
  switch (t.signal.get()) {
  | Anonymous => None
  | Authenticated(user) => Some(user)
  };

let is_authenticated = t =>
  switch (t.signal.get()) {
  | Anonymous => false
  | Authenticated(_) => true
  };

let has_role = (t, role) =>
  switch (t.signal.get()) {
  | Anonymous => false
  | Authenticated(user) => user.role == role
  };

let signal = t => t.signal;
