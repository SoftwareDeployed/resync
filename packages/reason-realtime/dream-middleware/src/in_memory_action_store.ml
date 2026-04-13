open Lwt.Syntax
open Mutation_result

let tbl = Hashtbl.create 128

let with_guard _db ~mutation_name ~action_id callback =
  let key = mutation_name ^ ":" ^ action_id in
  match Hashtbl.find_opt tbl key with
  | Some `Ok -> Lwt.return (Ack (Ok ()))
  | Some (`Failed msg) -> Lwt.return (Ack (Error msg))
  | None ->
    Lwt.catch
      (fun () ->
        let* result = callback () in
        match result with
        | Ack (Ok ()) ->
          Hashtbl.replace tbl key `Ok;
          Lwt.return (Ack (Ok ()))
        | Ack (Error msg) ->
          Hashtbl.replace tbl key (`Failed msg);
          Lwt.return (Ack (Error msg))
        | NoAck -> Lwt.return NoAck)
      (fun exn -> Lwt.fail exn)

let record_failed _db ~mutation_name ~action_id ~msg =
  let key = mutation_name ^ ":" ^ action_id in
  Hashtbl.replace tbl key (`Failed msg);
  Lwt.return (Ok ())

include (struct
  let with_guard = with_guard
  let record_failed = record_failed
end : Action_store.S)
