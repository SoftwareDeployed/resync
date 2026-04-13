val assoc : string -> Yojson.Basic.t -> Yojson.Basic.t option
val required_string : string -> Yojson.Basic.t -> (string, string) result
val required_bool : string -> Yojson.Basic.t -> (bool, string) result

type mutation_error =
  | Client_error of string
  | Caqti_error of Caqti_error.t
  | Internal_error of exn

val client_message_of_mutation_error : mutation_error -> string
val log_mutation_error : action_id:string -> mutation_error -> unit
val mutation_result : action_id:string -> unit Lwt.t -> (unit, mutation_error) result Lwt.t
val finish_mutation_result : action_id:string -> (unit, mutation_error) result -> Mutation_result.t Lwt.t
