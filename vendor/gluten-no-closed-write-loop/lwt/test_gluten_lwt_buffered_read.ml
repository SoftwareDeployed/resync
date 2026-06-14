open Lwt.Infix

module Runtime = struct
  type t =
    { mutable chunks : string list
    ; mutable shutdown_called : bool
    }

  let next_read_operation t =
    if List.length t.chunks >= 2 then `Close else `Read

  let read t bs ~off ~len =
    let n = min 2 len in
    if n > 0 then
      t.chunks <- t.chunks @ [ Bigstringaf.substring bs ~off ~len:n ];
    n

  let read_eof t bs ~off ~len =
    if len = 0 then 0 else read t bs ~off ~len

  let yield_reader _ _ =
    failwith "unexpected yield_reader"

  let next_write_operation _ = `Close 0
  let report_write_result _ _ = ()
  let yield_writer _ _ = ()
  let report_exn _ exn = raise exn
  let is_closed t = List.length t.chunks >= 2
  let shutdown t = t.shutdown_called <- true
end

module Io = struct
  type socket =
    { mutable reads : string list
    ; mutable read_calls : int
    ; mutable shutdown_receive_called : bool
    ; mutable closed : bool
    }

  type addr = unit

  let read socket bs ~off ~len =
    socket.read_calls <- socket.read_calls + 1;
    match socket.reads with
    | data :: rest ->
      let n = min (String.length data) len in
      Bigstringaf.blit_from_string data ~src_off:0 bs ~dst_off:off ~len:n;
      socket.reads <-
        if n = String.length data then
          rest
        else
          String.sub data n (String.length data - n) :: rest;
      Lwt.return n
    | [] ->
      let never, _wake = Lwt.wait () in
      never

  let writev _ _ = Lwt.return `Closed
  let shutdown_receive socket = socket.shutdown_receive_called <- true
  let close socket = socket.closed <- true; Lwt.return_unit
end

module Server = Gluten_lwt.Server (Io)

let failf fmt = Printf.ksprintf failwith fmt

let check name expected actual =
  if expected <> actual then
    failf "%s: expected %s, got %s" name expected actual

let check_int name expected actual =
  if expected <> actual then
    failf "%s: expected %d, got %d" name expected actual

let () =
  let runtime = { Runtime.chunks = []; shutdown_called = false } in
  let socket =
    { Io.reads = [ "abcd" ]
    ; read_calls = 0
    ; shutdown_receive_called = false
    ; closed = false
    }
  in
  let run =
    Server.create_connection_handler
      ~read_buffer_size:8
      ~protocol:(module Runtime)
      runtime
      ()
      socket
  in
  match
    Lwt_main.run
      (Lwt.pick
         [ (run >|= fun () -> `Finished)
         ; (Lwt_unix.sleep 0.2 >|= fun () -> `Timeout)
         ])
  with
  | `Timeout ->
    failwith "read loop blocked before draining buffered bytes"
  | `Finished ->
    check_int "socket reads" 1 socket.read_calls;
    check "first chunk" "ab" (List.nth runtime.chunks 0);
    check "second chunk" "cd" (List.nth runtime.chunks 1)
