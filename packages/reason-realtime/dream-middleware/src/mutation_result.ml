type t =
  | Ack of (unit, string) result
  | Ack_after_commit of (unit -> unit Lwt.t)
  | NoAck
