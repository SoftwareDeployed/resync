type user = {
  id: string,
  role: string,
};

type auth_state =
  | Anonymous
  | Authenticated(user);

type t;

let make: (~initial: auth_state=?, unit) => t;
let state: t => auth_state;
let set_state: (t, auth_state) => unit;
let update: (t, auth_state => auth_state) => unit;
let user: t => option(user);
let is_authenticated: t => bool;
let has_role: (t, string) => bool;
let signal: t => StoreSignal.t(auth_state);
