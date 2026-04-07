module type S = sig
  type t

  val start : t -> unit Lwt.t
  val stop : t -> unit Lwt.t
  val subscribe : t -> channel:string -> handler:(?wrap:(string -> string) -> string -> unit Lwt.t) -> unit Lwt.t
  val unsubscribe : t -> channel:string -> unit Lwt.t
end

type packed = Pack : (module S with type t = 'a) * 'a -> packed

let pack adapter value = Pack (adapter, value)

let start (Pack ((module Adapter), value)) = Adapter.start value
let stop (Pack ((module Adapter), value)) = Adapter.stop value

let subscribe (Pack ((module Adapter), value)) ~channel ~handler =
  Adapter.subscribe value ~channel ~handler

let unsubscribe (Pack ((module Adapter), value)) ~channel =
  Adapter.unsubscribe value ~channel
