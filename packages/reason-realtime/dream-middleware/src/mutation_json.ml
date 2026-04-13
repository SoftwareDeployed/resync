let assoc key = function
  | `Assoc fields -> List.assoc_opt key fields
  | _ -> None

let required_string key json =
  match assoc key json with
  | Some (`String value) -> Ok value
  | _ -> Error ("Missing string field: " ^ key)

let required_bool key json =
  match assoc key json with
  | Some (`Bool value) -> Ok value
  | _ -> Error ("Missing bool field: " ^ key)

let substring_after ~needle text =
  let text_length = String.length text in
  let needle_length = String.length needle in
  let rec loop index =
    if index + needle_length > text_length then
      None
    else if String.sub text index needle_length = needle then
      Some (String.sub text (index + needle_length) (text_length - index - needle_length))
    else
      loop (index + 1)
  in
  loop 0

let client_message_of_caqti_error error =
  let first_line =
    match String.split_on_char '\n' (Caqti_error.show error) with
    | line :: _ -> String.trim line
    | [] -> "Mutation failed"
  in
  match substring_after ~needle:"ERROR:" first_line with
  | Some message -> String.trim message
  | None ->
      (match substring_after ~needle:"failed:" first_line with
      | Some message -> String.trim message
      | None -> first_line)

type mutation_error =
  | Client_error of string
  | Caqti_error of Caqti_error.t
  | Internal_error of exn

let client_message_of_mutation_error = function
  | Client_error message -> message
  | Caqti_error error -> client_message_of_caqti_error error
  | Internal_error _ -> "Mutation failed"

let log_mutation_error ~action_id = function
  | Client_error message ->
      Printf.eprintf "Mutation rejected for action %s: %s\n%!" action_id message
  | Caqti_error error ->
      Printf.eprintf "Mutation failed for action %s: %s\n%!" action_id (Caqti_error.show error)
  | Internal_error exn ->
      Printf.eprintf "Mutation failed for action %s: %s\n%!" action_id (Printexc.to_string exn)

open Lwt.Syntax

let mutation_result ~action_id operation =
  Lwt.catch
    (fun () ->
      let* () = operation in
      Lwt.return (Ok ()))
    (function
      | Caqti_error.Exn error -> Lwt.return (Error (Caqti_error error))
      | exn -> Lwt.return (Error (Internal_error exn)))

let finish_mutation_result ~action_id result =
  match result with
  | Ok () -> Lwt.return (Mutation_result.Ack (Ok ()))
  | Error error ->
      log_mutation_error ~action_id error;
      Lwt.return (Mutation_result.Ack (Error (client_message_of_mutation_error error)))
