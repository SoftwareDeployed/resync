type user = {
  id : string;
  role : string;
}

type auth_state =
  | Anonymous
  | Authenticated of user

type t = {
  signal : auth_state StoreSignal.t;
}

let make ?(initial = Anonymous) () =
  { signal = StoreSignal.make initial }

let state t =
  t.signal.get ()

let set_state t next =
  t.signal.set next

let update t reducer =
  t.signal.update reducer

let user t =
  match t.signal.get () with
  | Anonymous -> None
  | Authenticated user -> Some user

let is_authenticated t =
  match t.signal.get () with
  | Anonymous -> false
  | Authenticated _ -> true

let has_role t role =
  match t.signal.get () with
  | Anonymous -> false
  | Authenticated user -> String.equal user.role role

let signal t =
  t.signal
