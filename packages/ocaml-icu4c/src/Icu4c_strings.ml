open Ctypes

let null_uchar = from_voidp uint16_t null
let null_char = from_voidp char null

let buffer_overflow_error = 15l

let check_error code =
  if Int32.compare code Int32.zero > 0 then
    let module I = Icu4c_bindings in
    failwith ("ICU error: " ^ I.u_errorName code)

let check_preflight_error code =
  if Int32.compare code Int32.zero > 0 && not (Int32.equal code buffer_overflow_error) then
    check_error code

let utf8_to_utf16 str =
  let module I = Icu4c_bindings in
  let dest_length = allocate int 0 in
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  ignore (I.u_strFromUTF8 null_uchar 0 dest_length str (String.length str) error_code);
  check_preflight_error !@error_code;
  let length = !@dest_length in
  let buffer = allocate_n uint16_t ~count:(length + 1) in
  error_code <-@ Int32.zero;
  ignore (I.u_strFromUTF8 buffer (length + 1) dest_length str (String.length str) error_code);
  check_error !@error_code;
  (buffer, !@dest_length)

let string_of_char_ptr ptr len =
  string_from_ptr ptr ~length:len

let utf16_to_utf8 ptr len =
  let module I = Icu4c_bindings in
  let dest_length = allocate int 0 in
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  ignore (I.u_strToUTF8 null_char 0 dest_length ptr len error_code);
  check_preflight_error !@error_code;
  let length = !@dest_length in
  let buffer = allocate_n char ~count:(length + 1) in
  error_code <-@ Int32.zero;
  ignore (I.u_strToUTF8 buffer (length + 1) dest_length ptr len error_code);
  check_error !@error_code;
  string_of_char_ptr buffer !@dest_length

let utf16_slice_to_utf8 buffer start_idx end_idx =
  utf16_to_utf8 (buffer +@ start_idx) (end_idx - start_idx)
