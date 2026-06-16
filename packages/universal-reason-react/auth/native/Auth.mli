type user = {
  id : string;
  role : string;
}

type auth_state =
  | Anonymous
  | Authenticated of user

type t

val make : ?initial:auth_state -> unit -> t
val state : t -> auth_state
val set_state : t -> auth_state -> unit
val update : t -> (auth_state -> auth_state) -> unit
val user : t -> user option
val is_authenticated : t -> bool
val has_role : t -> string -> bool
val signal : t -> auth_state StoreSignal.t
