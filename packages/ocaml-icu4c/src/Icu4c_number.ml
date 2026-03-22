open Ctypes

module Style = struct
  type t = Decimal | Currency | Percent
end

type options = {
  style : Style.t;
  currency : string option;
  locale : string;
  minimum_fraction_digits : int option;
  maximum_fraction_digits : int option;
  use_grouping : bool option;
}

type t = {
  ptr : unit ptr;
  mutable closed : bool;
}

let make_options ?(style = Style.Decimal) ?currency ?(locale = "en-US")
    ?minimum_fraction_digits ?maximum_fraction_digits ?use_grouping () =
  { style; currency; locale; minimum_fraction_digits; maximum_fraction_digits; use_grouping }

let build_skeleton opts =
  let buf = Buffer.create 64 in
  let add s = Buffer.add_string buf s in
  let space () = Buffer.add_char buf ' ' in
  (match opts.style with
    | Style.Currency ->
      (match opts.currency with
       | Some c -> add ("currency/" ^ c)
       | None -> add "currency")
    | Style.Decimal -> ()
    | Style.Percent ->
      add "percent";
      space ();
      add "scale/100";
      (match (opts.minimum_fraction_digits, opts.maximum_fraction_digits) with
       | None, None ->
         space ();
         add "precision-integer"
       | _ -> ())) ;
  (match (opts.minimum_fraction_digits, opts.maximum_fraction_digits) with
   | Some min, Some max when min = max ->
     space ();
     add ".";
     for _ = 1 to min do Buffer.add_char buf '0' done
   | Some min, Some max ->
     space ();
     add ".";
     for _ = 1 to min do Buffer.add_char buf '0' done;
     for _ = min + 1 to max do Buffer.add_char buf '#' done
   | Some min, None ->
     space ();
     add ".";
     for _ = 1 to min do Buffer.add_char buf '0' done;
     Buffer.add_char buf '*'
   | None, Some max ->
     space ();
     add ".";
     for _ = 1 to max do Buffer.add_char buf '#' done
   | None, None -> ());
  (match opts.use_grouping with
   | Some false -> space (); add "group-off"
   | _ -> ());
  Buffer.contents buf

let make opts =
  let module I = Icu4c_bindings in
  let skeleton = build_skeleton opts in
  let skeleton_utf16, skeleton_len = Icu4c_strings.utf8_to_utf16 skeleton in
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let ptr = I.unumf_openForSkeletonAndLocale
    skeleton_utf16
    skeleton_len
    opts.locale
    error_code in
  Icu4c_strings.check_error !@error_code;
  { ptr; closed = false }

let close fmt =
  let module I = Icu4c_bindings in
  if not fmt.closed then begin
    I.unumf_close fmt.ptr;
    fmt.closed <- true
  end

let format fmt value =
  let module I = Icu4c_bindings in
  if fmt.closed then failwith "Formatter has been closed";
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let result = I.unumf_openResult error_code in
  Icu4c_strings.check_error !@error_code;
  error_code <-@ Int32.zero;
  I.unumf_formatDouble fmt.ptr value result error_code;
  Icu4c_strings.check_error !@error_code;
  error_code <-@ Int32.zero;
  let result_buffer = allocate_n uint16_t ~count:256 in
  let len = I.unumf_resultToString result result_buffer 256 error_code in
  Icu4c_strings.check_error !@error_code;
  let str = Icu4c_strings.utf16_to_utf8 result_buffer len in
  I.unumf_closeResult result;
  str

let format_with_options opts value =
  let fmt = make opts in
  Fun.protect
    ~finally:(fun () -> close fmt)
    (fun () -> format fmt value)
