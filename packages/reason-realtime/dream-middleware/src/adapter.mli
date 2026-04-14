module type S = sig
  type t

  val start : t -> unit Lwt.t
  val stop : t -> unit Lwt.t
  val subscribe : t -> channel:string -> handler:(?wrap:(channel:string -> string -> string) -> string -> unit Lwt.t) -> unit Lwt.t
  val unsubscribe : t -> channel:string -> unit Lwt.t
end

type packed

val pack : (module S with type t = 'a) -> 'a -> packed
val start : packed -> unit Lwt.t
val stop : packed -> unit Lwt.t
val subscribe : packed -> channel:string -> handler:(?wrap:(channel:string -> string -> string) -> string -> unit Lwt.t) -> unit Lwt.t
val unsubscribe : packed -> channel:string -> unit Lwt.t
