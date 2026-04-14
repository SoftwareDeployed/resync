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

let current_registry : query_registry option ref = ref None

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
  current_registry := Some registry;
  let* result = f () in
  current_registry := None;
  Lwt.return result

let decode_results decodeRow json =
  match json with
  | `List items -> List.map decodeRow items
  | _ -> []

let register_query ~channel ~params ~sql ~decodeRow ~rowToJson param_type row_type =
  match !current_registry with
  | None -> None
  | Some registry ->
    let key = makeKey ~channel ~paramsHash:(hash_params params) in
    if Hashtbl.mem registry.queries key then
      match Hashtbl.find_opt registry.results key with
      | Some json -> Some (decode_results decodeRow json)
      | None -> None
    else begin
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
    end

let execute_queries () =
  match !current_registry with
  | None -> Lwt.return ()
  | Some registry ->
    match registry.state with
    | Collecting ->
      let queries = Hashtbl.to_seq_values registry.queries |> List.of_seq in
      let* () = Lwt_list.iter_s (fun (Query q) ->
        match registry.db_connection with
        | None -> Lwt.return ()
        | Some (module Db) ->
          Lwt.catch
            (fun () ->
              let* result = q.execute (module Db) in
              match result with
              | Stdlib.Ok json ->
                Hashtbl.add registry.results q.key json;
                Lwt.return ()
              | Stdlib.Error _ -> Lwt.return ())
            (fun _exn -> Lwt.return ())
      ) queries in
      registry.state <- Executed;
      Lwt.return ()
    | _ -> Lwt.return ()

let get_results () =
  match !current_registry with
  | None -> []
  | Some registry -> Hashtbl.to_seq registry.results |> List.of_seq
