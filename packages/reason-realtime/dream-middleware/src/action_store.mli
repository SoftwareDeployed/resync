module type S = sig
  val with_guard :
    (module Caqti_lwt.CONNECTION) ->
    mutation_name:string ->
    action_id:string ->
    (unit -> Mutation_result.t Lwt.t) ->
    Mutation_result.t Lwt.t

  val record_failed :
    (module Caqti_lwt.CONNECTION) ->
    mutation_name:string ->
    action_id:string ->
    msg:string ->
    (unit, Caqti_error.t) result Lwt.t
end
