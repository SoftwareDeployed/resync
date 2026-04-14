(* QueryRegistry.ml - Native SSR implementation *)

open Lwt.Syntax

(* Shared types for compatibility with QueryCache *)
type query_key = string

type 'a query_result =
  | Loading
  | Loaded of 'a array
  | Error of string

type ('params, 'row) query_spec = {
  channel : string;
  params : 'params;
  sql : string;
  decodeRow : StoreJson.json -> 'row;
}

type ('params, 'row) registered_query_spec = {
  key : query_key;
  spec : ('params, 'row) query_spec;
  mutable result : 'row query_result;
}

type registry_snapshot = {
  queries : query_key array;
  results : StoreJson.json Js.Dict.t;
}

let makeKey ~channel ~paramsHash =
  channel ^ ":" ^ paramsHash

(* SSR-specific types *)
type registry_state = Collecting | Executed | Rendered

type registered_query_exec =
  | Query : {
      key : string;
      channel : string;
      params : 'p;
      sql : string;
      execute : (module Caqti_lwt.CONNECTION) -> (Yojson.Safe.t, string) Stdlib.result Lwt.t;
      decode : Yojson.Safe.t -> 'a;
    } -> registered_query_exec

type query_registry = {
  mutable state : registry_state;
  mutable queries : (string, registered_query_exec) Hashtbl.t;
  mutable results : (string, Yojson.Safe.t) Hashtbl.t;
  db_connection : (module Caqti_lwt.CONNECTION) option;
}

(* Thread-local storage using Lwt key *)
let registry_key : query_registry Lwt.key = Lwt.new_key ()

let hash_params params =
  Marshal.to_string params []
  |> Digest.string
  |> Digest.to_hex

let with_registry ~db f =
  let registry = {
    state = Collecting;
    queries = Hashtbl.create 8;
    results = Hashtbl.create 8;
    db_connection = Some db;
  } in
  Lwt.with_value registry_key (Some registry) f

let decode_results decodeRow json =
  match json with
  | `List items -> List.map decodeRow items |> Array.of_list
  | _ -> [||]

let register_query ~channel ~params ~sql ~decodeRow ~rowToJson param_type row_type =
  match Lwt.get registry_key with
  | None -> None
  | Some registry ->
    let key = makeKey ~channel ~paramsHash:(hash_params params) in
    match Hashtbl.find_opt registry.queries key with
    | Some _ ->
      begin match Hashtbl.find_opt registry.results key with
      | Some json -> Some (decode_results decodeRow json)
      | None -> None
      end
    | None ->
      let request = Caqti_request.Infix.(param_type ->* row_type)(sql) in
      let execute (module Db : Caqti_lwt.CONNECTION) =
        let* result = Db.collect_list request params in
        match result with
        | Stdlib.Error err -> Lwt.return (Stdlib.Error (Caqti_error.show err))
        | Stdlib.Ok rows ->
          let json = `List (List.map rowToJson rows) in
          Lwt.return (Stdlib.Ok json)
      in
      Hashtbl.add registry.queries key (Query {
        key;
        channel;
        params;
        sql;
        execute;
        decode = decodeRow;
      });
      None

let execute_queries () =
  match Lwt.get registry_key with
  | None -> Lwt.return ()
  | Some registry ->
    match registry.state with
    | Collecting ->
      let queries = Hashtbl.to_seq_values registry.queries |> List.of_seq in
      let* () = Lwt_list.iter_p (fun (Query q) ->
        match registry.db_connection with
        | None -> Lwt.return ()
        | Some (module Db) ->
          Lwt.catch
            (fun () ->
              let* result = q.execute (module Db) in
              match result with
              | Stdlib.Ok json ->
                Hashtbl.replace registry.results q.key json;
                Lwt.return ()
              | Stdlib.Error _ -> Lwt.return ())
            (fun _exn -> Lwt.return ())
      ) queries in
      registry.state <- Executed;
      Lwt.return ()
    | _ -> Lwt.return ()

let get_results () =
  match Lwt.get registry_key with
  | None ->
    { queries = [||]; results = Js.Dict.empty () }
  | Some registry ->
    let queries = Hashtbl.to_seq_keys registry.queries |> Array.of_seq in
    let results = Js.Dict.empty () in
    Hashtbl.iter (fun key json ->
      Js.Dict.set results key (Obj.magic json)
    ) registry.results;
    { queries; results }
