open Ctypes

module Style = struct
  type t = Full | Long | Medium | Short
end

module Text = struct
  type t = Narrow | Short | Long
end

module Numeric = struct
  type t = Numeric | Two_digit
end

module Month = struct
  type t = Numeric | Two_digit | Narrow | Short | Long
end

module Hour_cycle = struct
  type t = H11 | H12 | H23 | H24
end

module Time_zone_name = struct
  type t =
    | Short
    | Long
    | Short_offset
    | Long_offset
    | Short_generic
    | Long_generic
end

type part = {
  type_ : string;
  value : string;
}

type options = {
  locale : string;
  time_zone : string option;
  date_style : Style.t option;
  time_style : Style.t option;
  weekday : Text.t option;
  era : Text.t option;
  year : Numeric.t option;
  month : Month.t option;
  day : Numeric.t option;
  hour : Numeric.t option;
  minute : Numeric.t option;
  second : Numeric.t option;
  fractional_second_digits : int option;
  time_zone_name : Time_zone_name.t option;
  hour12 : bool option;
  hour_cycle : Hour_cycle.t option;
}

type formatter = {
  ptr : unit ptr;
  mutable closed : bool;
}

let make_options ?(locale = "en-US") ?time_zone ?date_style ?time_style ?weekday ?era ?year ?month ?day ?hour ?minute ?second ?fractional_second_digits ?time_zone_name ?hour12 ?hour_cycle () =
  {
    locale;
    time_zone;
    date_style;
    time_style;
    weekday;
    era;
    year;
    month;
    day;
    hour;
    minute;
    second;
    fractional_second_digits;
    time_zone_name;
    hour12;
    hour_cycle;
  }

let null_uchar = from_voidp uint16_t null
let null_char = from_voidp char null
let null_void = from_voidp void null

let buffer_overflow_error = 15l
let pattern_match_all_fields_length = (1 lsl 17) - 1
let udat_none = -1
let udat_pattern = -2

let check_error code =
  if Int32.compare code Int32.zero > 0 then
    failwith ("ICU error: " ^ Datetime_icu.u_errorName code)

let check_preflight_error code =
  if Int32.compare code Int32.zero > 0 && not (Int32.equal code buffer_overflow_error) then
    check_error code

let string_of_char_ptr ptr len =
  string_from_ptr ptr ~length:len

let utf8_to_utf16 str =
  let dest_length = allocate int 0 in
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  ignore (Datetime_icu.u_strFromUTF8 null_uchar 0 dest_length str (String.length str) error_code);
  check_preflight_error !@error_code;
  let length = !@dest_length in
  let buffer = allocate_n uint16_t ~count:(length + 1) in
  error_code <-@ Int32.zero;
  ignore (Datetime_icu.u_strFromUTF8 buffer (length + 1) dest_length str (String.length str) error_code);
  check_error !@error_code;
  (buffer, !@dest_length)

let utf16_to_utf8 ptr len =
  let dest_length = allocate int 0 in
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  ignore (Datetime_icu.u_strToUTF8 null_char 0 dest_length ptr len error_code);
  check_preflight_error !@error_code;
  let length = !@dest_length in
  let buffer = allocate_n char ~count:(length + 1) in
  error_code <-@ Int32.zero;
  ignore (Datetime_icu.u_strToUTF8 buffer (length + 1) dest_length ptr len error_code);
  check_error !@error_code;
  string_of_char_ptr buffer !@dest_length

let option_is_some = function
  | Some _ -> true
  | None -> false

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
    let parsed_length = allocate int 0 in
    let error_code = allocate int32_t Int32.zero in
    error_code <-@ Int32.zero;
    let needed =
      Datetime_icu.uloc_forLanguageTag locale null_char 0 parsed_length error_code in
    check_preflight_error !@error_code;
    if !@parsed_length <> String.length locale then
      invalid_arg ("invalid locale: " ^ locale);
    let buffer = allocate_n char ~count:(needed + 1) in
    error_code <-@ Int32.zero;
    let actual =
      Datetime_icu.uloc_forLanguageTag locale buffer (needed + 1) parsed_length error_code in
    check_error !@error_code;
    if !@parsed_length <> String.length locale then
      invalid_arg ("invalid locale: " ^ locale);
    string_of_char_ptr buffer actual

let normalize_time_zone time_zone =
  if String.length time_zone = 0 then invalid_arg "time_zone must not be empty";
  let input, input_length = utf8_to_utf16 time_zone in
  let is_system_id = allocate char '\000' in
  let error_code = allocate int32_t Int32.zero in
  let initial_capacity = 128 in
  let buffer = allocate_n uint16_t ~count:initial_capacity in
  error_code <-@ Int32.zero;
  let actual =
    Datetime_icu.ucal_getCanonicalTimeZoneID input input_length buffer initial_capacity is_system_id error_code in
  if Int32.equal !@error_code buffer_overflow_error then begin
    let larger = allocate_n uint16_t ~count:(actual + 1) in
    error_code <-@ Int32.zero;
    let actual =
      Datetime_icu.ucal_getCanonicalTimeZoneID input input_length larger (actual + 1) is_system_id error_code in
    check_error !@error_code;
    utf16_to_utf8 larger actual
  end else begin
    check_error !@error_code;
    utf16_to_utf8 buffer actual
  end

let has_component_options opts =
  List.exists Fun.id
    [ option_is_some opts.weekday
    ; option_is_some opts.era
    ; option_is_some opts.year
    ; option_is_some opts.month
    ; option_is_some opts.day
    ; option_is_some opts.hour
    ; option_is_some opts.minute
    ; option_is_some opts.second
    ; option_is_some opts.fractional_second_digits
    ; option_is_some opts.time_zone_name ]

let normalize_options opts =
  (match opts.fractional_second_digits with
   | Some digits when digits < 1 || digits > 3 ->
     invalid_arg "fractional_second_digits must be between 1 and 3"
   | _ -> ());
  if (option_is_some opts.date_style || option_is_some opts.time_style) && has_component_options opts then
    invalid_arg "date_style/time_style cannot be combined with component options";
  let second =
    match (opts.second, opts.fractional_second_digits) with
    | None, Some _ -> Some Numeric.Numeric
    | second, None -> second
    | Some second, Some _ -> Some second in
  let default_fields =
    not (option_is_some opts.date_style)
    && not (option_is_some opts.time_style)
    && not (has_component_options { opts with second }) in
  {
    locale = normalize_locale opts.locale;
    time_zone = Option.map normalize_time_zone opts.time_zone;
    date_style = opts.date_style;
    time_style = opts.time_style;
    weekday = opts.weekday;
    era = opts.era;
    year = (if default_fields then Some Numeric.Numeric else opts.year);
    month = (if default_fields then Some Month.Numeric else opts.month);
    day = (if default_fields then Some Numeric.Numeric else opts.day);
    hour = opts.hour;
    minute = opts.minute;
    second;
    fractional_second_digits = opts.fractional_second_digits;
    time_zone_name = opts.time_zone_name;
    hour12 = opts.hour12;
    hour_cycle = (match opts.hour12 with Some _ -> None | None -> opts.hour_cycle);
  }

let style_to_icu = function
  | None -> udat_none
  | Some Style.Full -> 0
  | Some Style.Long -> 1
  | Some Style.Medium -> 2
  | Some Style.Short -> 3

let text_key = function
  | Text.Narrow -> "n"
  | Text.Short -> "s"
  | Text.Long -> "l"

let numeric_key = function
  | Numeric.Numeric -> "n"
  | Numeric.Two_digit -> "2"

let month_key = function
  | Month.Numeric -> "n"
  | Month.Two_digit -> "2"
  | Month.Narrow -> "r"
  | Month.Short -> "s"
  | Month.Long -> "l"

let hour_cycle_key = function
  | Hour_cycle.H11 -> "11"
  | Hour_cycle.H12 -> "12"
  | Hour_cycle.H23 -> "23"
  | Hour_cycle.H24 -> "24"

let time_zone_name_key = function
  | Time_zone_name.Short -> "s"
  | Time_zone_name.Long -> "l"
  | Time_zone_name.Short_offset -> "so"
  | Time_zone_name.Long_offset -> "lo"
  | Time_zone_name.Short_generic -> "sg"
  | Time_zone_name.Long_generic -> "lg"

let cache_key opts =
  let buf = Buffer.create 96 in
  let add_opt key value =
    Buffer.add_string buf key;
    Buffer.add_char buf '=';
    Buffer.add_string buf value;
    Buffer.add_char buf '|' in
  add_opt "locale" opts.locale;
  add_opt "tz"
    (match opts.time_zone with
     | None -> "_"
     | Some time_zone -> time_zone);
  add_opt "ds"
    (match opts.date_style with
     | None -> "_"
     | Some Style.Full -> "f"
     | Some Style.Long -> "l"
     | Some Style.Medium -> "m"
     | Some Style.Short -> "s");
  add_opt "ts"
    (match opts.time_style with
     | None -> "_"
     | Some Style.Full -> "f"
     | Some Style.Long -> "l"
     | Some Style.Medium -> "m"
     | Some Style.Short -> "s");
  add_opt "weekday"
    (match opts.weekday with
     | None -> "_"
     | Some value -> text_key value);
  add_opt "era"
    (match opts.era with
     | None -> "_"
     | Some value -> text_key value);
  add_opt "year"
    (match opts.year with
     | None -> "_"
     | Some value -> numeric_key value);
  add_opt "month"
    (match opts.month with
     | None -> "_"
     | Some value -> month_key value);
  add_opt "day"
    (match opts.day with
     | None -> "_"
     | Some value -> numeric_key value);
  add_opt "hour"
    (match opts.hour with
     | None -> "_"
     | Some value -> numeric_key value);
  add_opt "minute"
    (match opts.minute with
     | None -> "_"
     | Some value -> numeric_key value);
  add_opt "second"
    (match opts.second with
     | None -> "_"
     | Some value -> numeric_key value);
  add_opt "fsd"
    (match opts.fractional_second_digits with
     | None -> "_"
     | Some value -> string_of_int value);
  add_opt "tzn"
    (match opts.time_zone_name with
     | None -> "_"
     | Some value -> time_zone_name_key value);
  add_opt "hour12"
    (match opts.hour12 with
     | None -> "_"
     | Some true -> "t"
     | Some false -> "f");
  add_opt "hour_cycle"
    (match opts.hour_cycle with
     | None -> "_"
     | Some value -> hour_cycle_key value);
  Buffer.contents buf

let hour_cycle_of_icu value =
  match value with
  | 0 -> Hour_cycle.H11
  | 1 -> Hour_cycle.H12
  | 2 -> Hour_cycle.H23
  | 3 -> Hour_cycle.H24
  | _ -> Hour_cycle.H23

let default_hour_cycle dtpg =
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let value = Datetime_icu.udatpg_getDefaultHourCycle dtpg error_code in
  check_error !@error_code;
  hour_cycle_of_icu value

let resolve_hour_cycle dtpg opts =
  match opts.hour with
  | None -> None
  | Some _ ->
    let locale_default = lazy (default_hour_cycle dtpg) in
    let cycle =
      match opts.hour12 with
      | Some true ->
        (match Lazy.force locale_default with
         | Hour_cycle.H11 -> Hour_cycle.H11
         | _ -> Hour_cycle.H12)
      | Some false ->
        (match Lazy.force locale_default with
         | Hour_cycle.H24 -> Hour_cycle.H24
         | _ -> Hour_cycle.H23)
      | None ->
        (match opts.hour_cycle with
         | Some cycle -> cycle
         | None -> Lazy.force locale_default) in
    Some cycle

let add_repeated buffer chr count =
  for _ = 1 to count do
    Buffer.add_char buffer chr
  done

let build_skeleton opts resolved_hour_cycle =
  let buffer = Buffer.create 32 in
  let add string = Buffer.add_string buffer string in
  (match opts.weekday with
   | None -> ()
   | Some Text.Narrow -> add "EEEEE"
   | Some Text.Short -> add "EEE"
   | Some Text.Long -> add "EEEE");
  (match opts.era with
   | None -> ()
   | Some Text.Narrow -> add "GGGGG"
   | Some Text.Short -> add "GGG"
   | Some Text.Long -> add "GGGG");
  (match opts.year with
   | None -> ()
   | Some Numeric.Numeric -> add "y"
   | Some Numeric.Two_digit -> add "yy");
  (match opts.month with
   | None -> ()
   | Some Month.Numeric -> add "M"
   | Some Month.Two_digit -> add "MM"
   | Some Month.Narrow -> add "MMMMM"
   | Some Month.Short -> add "MMM"
   | Some Month.Long -> add "MMMM");
  (match opts.day with
   | None -> ()
   | Some Numeric.Numeric -> add "d"
   | Some Numeric.Two_digit -> add "dd");
  (match (opts.hour, resolved_hour_cycle) with
   | None, _ -> ()
   | Some Numeric.Numeric, Some Hour_cycle.H11 -> add "K"
   | Some Numeric.Two_digit, Some Hour_cycle.H11 -> add "KK"
   | Some Numeric.Numeric, Some Hour_cycle.H12 -> add "h"
   | Some Numeric.Two_digit, Some Hour_cycle.H12 -> add "hh"
   | Some Numeric.Numeric, Some Hour_cycle.H23 -> add "H"
   | Some Numeric.Two_digit, Some Hour_cycle.H23 -> add "HH"
   | Some Numeric.Numeric, Some Hour_cycle.H24 -> add "k"
   | Some Numeric.Two_digit, Some Hour_cycle.H24 -> add "kk"
   | Some _, None -> ());
  (match opts.minute with
   | None -> ()
   | Some Numeric.Numeric -> add "m"
   | Some Numeric.Two_digit -> add "mm");
  (match opts.second with
   | None -> ()
   | Some Numeric.Numeric -> add "s"
   | Some Numeric.Two_digit -> add "ss");
  (match opts.fractional_second_digits with
   | None -> ()
   | Some digits -> add_repeated buffer 'S' digits);
  (match opts.time_zone_name with
   | None -> ()
   | Some Time_zone_name.Short -> add "z"
   | Some Time_zone_name.Long -> add "zzzz"
   | Some Time_zone_name.Short_offset -> add "O"
   | Some Time_zone_name.Long_offset -> add "OOOO"
   | Some Time_zone_name.Short_generic -> add "v"
   | Some Time_zone_name.Long_generic -> add "vvvv");
  Buffer.contents buffer

let best_pattern dtpg skeleton =
  let skeleton_ptr, skeleton_length = utf8_to_utf16 skeleton in
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let needed =
    Datetime_icu.udatpg_getBestPatternWithOptions dtpg skeleton_ptr skeleton_length pattern_match_all_fields_length null_uchar 0 error_code in
  check_preflight_error !@error_code;
  let pattern = allocate_n uint16_t ~count:(needed + 1) in
  error_code <-@ Int32.zero;
  let actual =
    Datetime_icu.udatpg_getBestPatternWithOptions dtpg skeleton_ptr skeleton_length pattern_match_all_fields_length pattern (needed + 1) error_code in
  check_error !@error_code;
  (pattern, actual)

let time_zone_ptr_and_length = function
  | None -> (null_uchar, 0)
  | Some time_zone -> utf8_to_utf16 time_zone

let create_style_formatter opts =
  let time_zone_ptr, time_zone_length = time_zone_ptr_and_length opts.time_zone in
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let ptr =
    Datetime_icu.udat_open
      (style_to_icu opts.time_style)
      (style_to_icu opts.date_style)
      opts.locale
      time_zone_ptr
      time_zone_length
      null_uchar
      0
      error_code in
  check_error !@error_code;
  { ptr; closed = false }

let create_pattern_formatter opts =
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let dtpg = Datetime_icu.udatpg_open opts.locale error_code in
  check_error !@error_code;
  Fun.protect
    ~finally:(fun () -> Datetime_icu.udatpg_close dtpg)
    (fun () ->
      let resolved_hour_cycle = resolve_hour_cycle dtpg opts in
      let skeleton = build_skeleton opts resolved_hour_cycle in
      let pattern, pattern_length = best_pattern dtpg skeleton in
      let time_zone_ptr, time_zone_length = time_zone_ptr_and_length opts.time_zone in
      error_code <-@ Int32.zero;
      let ptr =
        Datetime_icu.udat_open
          udat_pattern
          udat_pattern
          opts.locale
          time_zone_ptr
          time_zone_length
          pattern
          pattern_length
          error_code in
      check_error !@error_code;
      { ptr; closed = false })

let create_formatter opts =
  match (opts.date_style, opts.time_style) with
  | Some _, _ | _, Some _ -> create_style_formatter opts
  | None, None -> create_pattern_formatter opts

let close_formatter formatter =
  if not formatter.closed then begin
    Datetime_icu.udat_close formatter.ptr;
    formatter.closed <- true
  end

let format_with_function formatter format_fn =
  if formatter.closed then failwith "Formatter has been closed";
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let needed = format_fn null_uchar 0 error_code in
  check_preflight_error !@error_code;
  let buffer = allocate_n uint16_t ~count:(needed + 1) in
  error_code <-@ Int32.zero;
  let actual = format_fn buffer (needed + 1) error_code in
  check_error !@error_code;
  (buffer, actual)

let format_formatter formatter value =
  let buffer, length =
    format_with_function formatter (fun out capacity error_code ->
      Datetime_icu.udat_format formatter.ptr value out capacity null_void error_code) in
  utf16_to_utf8 buffer length

let part_type_of_field = function
  | 0 -> "era"
  | 1 | 18 | 20 -> "year"
  | 2 | 26 -> "month"
  | 3 -> "day"
  | 4 | 5 | 15 | 16 -> "hour"
  | 6 -> "minute"
  | 7 -> "second"
  | 8 -> "fractionalSecond"
  | 9 | 25 -> "weekday"
  | 14 | 35 | 36 -> "dayPeriod"
  | 17 | 23 | 24 | 29 | 31 | 32 | 33 -> "timeZoneName"
  | 30 -> "yearName"
  | 34 -> "relatedYear"
  | _ -> "literal"

let utf16_slice_to_utf8 buffer start_idx end_idx =
  utf16_to_utf8 (buffer +@ start_idx) (end_idx - start_idx)

let build_parts buffer length fields =
  let add_part acc type_ value =
    if String.length value = 0 then acc else { type_; value } :: acc in
  let rec loop cursor acc = function
    | [] ->
      let acc =
        if cursor < length then
          add_part acc "literal" (utf16_slice_to_utf8 buffer cursor length)
        else
          acc in
      List.rev acc
    | (field, begin_idx, end_idx) :: rest ->
      let acc =
        if begin_idx > cursor then
          add_part acc "literal" (utf16_slice_to_utf8 buffer cursor begin_idx)
        else
          acc in
      let segment_start = max cursor begin_idx in
      let acc =
        if end_idx > segment_start then
          add_part acc (part_type_of_field field) (utf16_slice_to_utf8 buffer segment_start end_idx)
        else
          acc in
      loop (max cursor end_idx) acc rest in
  loop 0 [] fields

let collect_fields iterator =
  let rec loop acc =
    let begin_idx = allocate int 0 in
    let end_idx = allocate int 0 in
    let field = Datetime_icu.ufieldpositer_next iterator begin_idx end_idx in
    if field < 0 then
      List.rev acc
    else
      loop ((field, !@begin_idx, !@end_idx) :: acc) in
  loop []

let format_to_parts_formatter formatter value =
  if formatter.closed then failwith "Formatter has been closed";
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let iterator = Datetime_icu.ufieldpositer_open error_code in
  check_error !@error_code;
  Fun.protect
    ~finally:(fun () -> Datetime_icu.ufieldpositer_close iterator)
    (fun () ->
      let buffer, length =
        format_with_function formatter (fun out capacity error_code ->
          Datetime_icu.udat_formatForFields formatter.ptr value out capacity iterator error_code) in
      let fields = collect_fields iterator in
      build_parts buffer length fields)

let cache : (string, formatter) Hashtbl.t = Hashtbl.create 16

let get_or_create_formatter opts =
  let key = cache_key opts in
  match Hashtbl.find_opt cache key with
  | Some formatter -> formatter
  | None ->
    let formatter = create_formatter opts in
    Hashtbl.add cache key formatter;
    formatter

let format_with_options opts value =
  let normalized = normalize_options opts in
  let formatter = get_or_create_formatter normalized in
  format_formatter formatter value

let format_to_parts_with_options opts value =
  let normalized = normalize_options opts in
  let formatter = get_or_create_formatter normalized in
  format_to_parts_formatter formatter value

let format ?locale ?time_zone ?date_style ?time_style ?weekday ?era ?year ?month ?day ?hour ?minute ?second ?fractional_second_digits ?time_zone_name ?hour12 ?hour_cycle value =
  let opts =
    make_options ?locale ?time_zone ?date_style ?time_style ?weekday ?era ?year ?month ?day ?hour ?minute ?second ?fractional_second_digits ?time_zone_name ?hour12 ?hour_cycle () in
  format_with_options opts value

let format_to_parts ?locale ?time_zone ?date_style ?time_style ?weekday ?era ?year ?month ?day ?hour ?minute ?second ?fractional_second_digits ?time_zone_name ?hour12 ?hour_cycle value =
  let opts =
    make_options ?locale ?time_zone ?date_style ?time_style ?weekday ?era ?year ?month ?day ?hour ?minute ?second ?fractional_second_digits ?time_zone_name ?hour12 ?hour_cycle () in
  format_to_parts_with_options opts value

let () =
  at_exit (fun () -> Hashtbl.iter (fun _ formatter -> close_formatter formatter) cache)
