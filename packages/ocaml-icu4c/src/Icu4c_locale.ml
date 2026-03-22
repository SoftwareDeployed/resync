open Ctypes

let contains_char str chr =
  try
    ignore (String.index str chr);
    true
  with Not_found ->
    false

let normalize_locale locale =
  if String.length locale = 0 then invalid_arg "locale must not be empty";
  if contains_char locale '_' || contains_char locale '@' then
    locale
  else
    let module I = Icu4c_bindings in
    let parsed_length = allocate int 0 in
    let error_code = allocate int32_t Int32.zero in
    error_code <-@ Int32.zero;
    let needed =
      I.uloc_forLanguageTag locale Icu4c_strings.null_char 0 parsed_length error_code in
    Icu4c_strings.check_preflight_error !@error_code;
    if !@parsed_length <> String.length locale then
      invalid_arg ("invalid locale: " ^ locale);
    let buffer = allocate_n char ~count:(needed + 1) in
    error_code <-@ Int32.zero;
    let actual =
      I.uloc_forLanguageTag locale buffer (needed + 1) parsed_length error_code in
    Icu4c_strings.check_error !@error_code;
    if !@parsed_length <> String.length locale then
      invalid_arg ("invalid locale: " ^ locale);
    Icu4c_strings.string_of_char_ptr buffer actual

let normalize_time_zone time_zone =
  if String.length time_zone = 0 then invalid_arg "time_zone must not be empty";
  let module I = Icu4c_bindings in
  let input, input_length = Icu4c_strings.utf8_to_utf16 time_zone in
  let is_system_id = allocate char '\000' in
  let error_code = allocate int32_t Int32.zero in
  let initial_capacity = 128 in
  let buffer = allocate_n uint16_t ~count:initial_capacity in
  error_code <-@ Int32.zero;
  let actual =
    I.ucal_getCanonicalTimeZoneID input input_length buffer initial_capacity is_system_id error_code in
  if Int32.equal !@error_code Icu4c_strings.buffer_overflow_error then begin
    let larger = allocate_n uint16_t ~count:(actual + 1) in
    error_code <-@ Int32.zero;
    let actual =
      I.ucal_getCanonicalTimeZoneID input input_length larger (actual + 1) is_system_id error_code in
    Icu4c_strings.check_error !@error_code;
    Icu4c_strings.utf16_to_utf8 larger actual
  end else begin
    Icu4c_strings.check_error !@error_code;
    Icu4c_strings.utf16_to_utf8 buffer actual
  end
