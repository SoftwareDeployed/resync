exception Unused_db_access of string

let fail name = Lwt.fail (Unused_db_access name)
let fail_sync name = raise (Unused_db_access name)

module Fail_fast_connection = struct
  let driver_info = Caqti_driver_info.dummy
  let dialect = Caqti_driver_info.dummy_dialect driver_info
  let driver_connection = None

  module Response = struct
    type 'a fiber = 'a Lwt.t
    type ('a, 'err) stream = ('a, 'err) Caqti_lwt.Stream.t
    type ('b, 'm) t = unit

    let returned_count _ = fail "Response.returned_count"
    let affected_count _ = fail "Response.affected_count"
    let exec _ = fail "Response.exec"
    let find _ = fail "Response.find"
    let find_opt _ = fail "Response.find_opt"
    let fold _ _ _ = fail "Response.fold"
    let fold_s _ _ _ = fail "Response.fold_s"
    let iter_s _ _ = fail "Response.iter_s"
    let to_stream _ = fail_sync "Response.to_stream"
  end

  let call ~f:_ _request _params = fail "call"
  let set_statement_timeout _ = fail "set_statement_timeout"
  let start () = fail "start"
  let commit () = fail "commit"
  let rollback () = fail "rollback"
  let deallocate _ = fail "deallocate"
  let disconnect () = fail "disconnect"
  let validate () = fail "validate"
  let check _ = fail_sync "check"
  let exec _ _ = fail "exec"
  let exec_with_affected_count _ _ = fail "exec_with_affected_count"
  let find _ _ = fail "find"
  let find_opt _ _ = fail "find_opt"
  let fold _ _ _ _ = fail "fold"
  let fold_s _ _ _ _ = fail "fold_s"
  let iter_s _ _ _ = fail "iter_s"
  let collect_list _ _ = fail "collect_list"
  let rev_collect_list _ _ = fail "rev_collect_list"
  let with_transaction _ = fail "with_transaction"
  let populate ~table:_ ~columns:_ _ _ = fail "populate"
end

let unused : (module Caqti_lwt.CONNECTION) =
  (module Fail_fast_connection : Caqti_lwt.CONNECTION)

let use_unused _request callback = callback unused
