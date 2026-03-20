open Ctypes

type style =
  | Currency
  | Decimal
  | Percent

type options = {
  style : style;
  currency : string option;
  locale : string;
  minimum_fraction_digits : int option;
  maximum_fraction_digits : int option;
  use_grouping : bool option;
}

type formatter = {
  ptr : unit ptr;
  mutable closed : bool;
}

let make_options ?(style = Decimal) ?currency ?(locale = "en-US")
    ?minimum_fraction_digits ?maximum_fraction_digits ?use_grouping () =
  { style; currency; locale; minimum_fraction_digits; maximum_fraction_digits; use_grouping }

let build_skeleton opts =
  let buf = Buffer.create 64 in
  let add s = Buffer.add_string buf s in
  let space () = Buffer.add_char buf ' ' in
  (match opts.style with
    | Currency ->
      (match opts.currency with
       | Some c -> add ("currency/" ^ c)
       | None -> add "currency")
    | Decimal -> ()
    | Percent ->
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

let utf8_to_utf16 str =
  let len = String.length str in
  let buf = allocate_n uint16_t ~count:(len + 1) in
  for i = 0 to len - 1 do
    let c = Char.code (String.get str i) in
    (buf +@ i) <-@ Unsigned.UInt16.of_int c
  done;
  buf

let error_code = allocate int32_t Int32.zero

let check_error code =
  if Int32.compare code Int32.zero > 0 then
    let msg = Icu.u_errorName code in
    failwith ("ICU error: " ^ msg)

let create_formatter opts =
  let skeleton = build_skeleton opts in
  let skeleton_utf16 = utf8_to_utf16 skeleton in
  let skeleton_len = String.length skeleton in
  error_code <-@ Int32.zero;
  let ptr = Icu.unumf_openForSkeletonAndLocale
    skeleton_utf16
    skeleton_len
    opts.locale
    error_code in
  check_error !@error_code;
  { ptr; closed = false }

let close_formatter fmt =
  if not fmt.closed then begin
    Icu.unumf_close fmt.ptr;
    fmt.closed <- true
  end

let utf16_to_utf8 ustr len =
  let rec loop i acc =
    if i >= len then List.rev acc
    else begin
      let hi = Unsigned.UInt16.to_int (!@(ustr +@ i)) in
      if hi land 0xFC00 = 0xD800 && i + 1 < len then begin
        let lo = Unsigned.UInt16.to_int (!@(ustr +@ (i + 1))) in
        if lo land 0xFC00 = 0xDC00 then begin
          let cp = ((hi land 0x3FF) lsl 10) + (lo land 0x3FF) + 0x10000 in
          loop (i + 2) (cp :: acc)
        end else
          loop (i + 1) (hi :: acc)
      end else
        loop (i + 1) (hi :: acc)
    end
  in
  let codepoints = loop 0 [] in
  let buf = Buffer.create (len * 3) in
  List.iter (fun cp ->
    if cp < 0x80 then
      Buffer.add_char buf (Char.chr cp)
    else if cp < 0x800 then begin
      Buffer.add_char buf (Char.chr (0xC0 lor (cp lsr 6)));
      Buffer.add_char buf (Char.chr (0x80 lor (cp land 0x3F)))
    end else if cp < 0x10000 then begin
      Buffer.add_char buf (Char.chr (0xE0 lor (cp lsr 12)));
      Buffer.add_char buf (Char.chr (0x80 lor ((cp lsr 6) land 0x3F)));
      Buffer.add_char buf (Char.chr (0x80 lor (cp land 0x3F)))
    end else begin
      Buffer.add_char buf (Char.chr (0xF0 lor (cp lsr 18)));
      Buffer.add_char buf (Char.chr (0x80 lor ((cp lsr 12) land 0x3F)));
      Buffer.add_char buf (Char.chr (0x80 lor ((cp lsr 6) land 0x3F)));
      Buffer.add_char buf (Char.chr (0x80 lor (cp land 0x3F)))
    end)
    codepoints;
  Buffer.contents buf

let result_buffer = allocate_n uint16_t ~count:256
let error_code2 = allocate int32_t Int32.zero

let format fmt value =
  if fmt.closed then failwith "Formatter has been closed";
  error_code2 <-@ Int32.zero;
  let result = Icu.unumf_openResult error_code2 in
  check_error !@error_code2;
  error_code2 <-@ Int32.zero;
  Icu.unumf_formatDouble fmt.ptr value result error_code2;
  check_error !@error_code2;
  error_code2 <-@ Int32.zero;
  let len = Icu.unumf_resultToString result result_buffer 256 error_code2 in
  check_error !@error_code2;
  let str = utf16_to_utf8 result_buffer len in
  Icu.unumf_closeResult result;
  str

let cache : (string, formatter) Hashtbl.t = Hashtbl.create 16

let cache_key opts =
  let buf = Buffer.create 32 in
  Buffer.add_string buf opts.locale;
  Buffer.add_char buf '|';
  (match opts.style with
   | Currency -> Buffer.add_string buf "C"
   | Decimal -> Buffer.add_string buf "D"
   | Percent -> Buffer.add_string buf "P");
  Buffer.add_char buf '|';
  (match opts.currency with
   | None -> Buffer.add_string buf "_"
   | Some c -> Buffer.add_string buf c);
  Buffer.add_char buf '|';
  (match opts.minimum_fraction_digits with
   | None -> Buffer.add_string buf "_"
   | Some n -> Buffer.add_string buf (string_of_int n));
  Buffer.add_char buf '|';
  (match opts.maximum_fraction_digits with
   | None -> Buffer.add_string buf "_"
   | Some n -> Buffer.add_string buf (string_of_int n));
  Buffer.add_char buf '|';
  (match opts.use_grouping with
   | None -> Buffer.add_string buf "_"
   | Some true -> Buffer.add_string buf "T"
   | Some false -> Buffer.add_string buf "F");
  Buffer.contents buf

let get_or_create_formatter opts =
  let key = cache_key opts in
  match Hashtbl.find_opt cache key with
  | Some fmt -> fmt
  | None ->
    let fmt = create_formatter opts in
    Hashtbl.add cache key fmt;
    fmt

let format_with_options opts value =
  let fmt = get_or_create_formatter opts in
  format fmt value

let format_currency ?locale ?currency ?min_fraction ?max_fraction value =
  let opts = make_options
    ~style:Currency
    ?locale
    ?currency
    ?minimum_fraction_digits:min_fraction
    ?maximum_fraction_digits:max_fraction
    () in
  format_with_options opts value

let format_decimal ?locale ?min_fraction ?max_fraction ?grouping value =
  let opts = make_options
    ~style:Decimal
    ?locale
    ?minimum_fraction_digits:min_fraction
    ?maximum_fraction_digits:max_fraction
    ?use_grouping:grouping
    () in
  format_with_options opts value

let format_percent ?locale ?min_fraction ?max_fraction value =
  let opts = make_options
    ~style:Percent
    ?locale
    ?minimum_fraction_digits:min_fraction
    ?maximum_fraction_digits:max_fraction
    () in
  format_with_options opts value

let () = at_exit (fun () ->
  Hashtbl.iter (fun _ fmt -> close_formatter fmt) cache)
