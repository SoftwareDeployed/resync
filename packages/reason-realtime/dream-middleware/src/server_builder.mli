type t

val make :
  ?doc_root:string ->
  ?doc_root_var:string ->
  ?db_uri:string ->
  ?db_url_var:string ->
  ?interface:string ->
  ?interface_var:string ->
  ?port:int ->
  ?port_var:string ->
  ?default_interface:string ->
  ?default_port:int ->
  unit -> t

val doc_root : t -> string
val db_uri : t -> string option

val with_packed_adapter : Adapter.packed -> t -> t

val with_middleware :
  resolve_subscription:(Dream.request -> string -> string option Lwt.t) ->
  load_snapshot:(Dream.request -> string -> string Lwt.t) ->
  ?handle_mutation:(Middleware.broadcast_fn -> Dream.request -> db:(module Caqti_lwt.CONNECTION) -> action_id:string -> mutation_name:string -> Yojson.Basic.t -> Mutation_result.t Lwt.t) ->
  ?validate_mutation:(Dream.request -> Yojson.Basic.t -> (unit, string) result Lwt.t) ->
  ?handle_media:(Middleware.broadcast_fn -> Dream.request -> string -> string -> (unit, string) result Lwt.t) ->
  ?handle_disconnect:(Middleware.broadcast_fn -> string -> unit Lwt.t) ->
  t -> t

val with_routes : Dream.route list -> t -> t

val with_pre_start : (unit -> unit) -> t -> t

val run : t -> unit
