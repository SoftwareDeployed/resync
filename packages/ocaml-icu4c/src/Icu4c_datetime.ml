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

type t = {
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

let null_void = from_voidp void null

let udat_none = -1
let udat_pattern = -2

let style_to_icu = function
  | None -> udat_none
  | Some Style.Full -> 0
  | Some Style.Long -> 1
  | Some Style.Medium -> 2
  | Some Style.Short -> 3

let option_is_some = function
  | Some _ -> true
  | None -> false

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
    locale = Icu4c_locale.normalize_locale opts.locale;
    time_zone = Option.map Icu4c_locale.normalize_time_zone opts.time_zone;
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

let pattern_match_all_fields_length = (1 lsl 17) - 1

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
   | Some digits -> for _ = 1 to digits do Buffer.add_char buffer 'S' done);
  (match opts.time_zone_name with
   | None -> ()
   | Some Time_zone_name.Short -> add "z"
   | Some Time_zone_name.Long -> add "zzzz"
   | Some Time_zone_name.Short_offset -> add "O"
   | Some Time_zone_name.Long_offset -> add "OOOO"
   | Some Time_zone_name.Short_generic -> add "v"
   | Some Time_zone_name.Long_generic -> add "vvvv");
  Buffer.contents buffer

let make opts =
  let module I = Icu4c_bindings in
  let normalized = normalize_options opts in
  let time_zone_ptr, time_zone_length =
    match normalized.time_zone with
    | None -> (Icu4c_strings.null_uchar, 0)
    | Some time_zone -> Icu4c_strings.utf8_to_utf16 time_zone in
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let ptr =
    I.udat_open
      (style_to_icu normalized.time_style)
      (style_to_icu normalized.date_style)
      normalized.locale
      time_zone_ptr
      time_zone_length
      Icu4c_strings.null_uchar
      0
      error_code in
  Icu4c_strings.check_error !@error_code;
  { ptr; closed = false }

let close formatter =
  let module I = Icu4c_bindings in
  if not formatter.closed then begin
    I.udat_close formatter.ptr;
    formatter.closed <- true
  end

let format formatter value =
  let module I = Icu4c_bindings in
  if formatter.closed then failwith "Formatter has been closed";
  let error_code = allocate int32_t Int32.zero in
  error_code <-@ Int32.zero;
  let needed =
    I.udat_format formatter.ptr value Icu4c_strings.null_uchar 0 null_void error_code in
  Icu4c_strings.check_preflight_error !@error_code;
  let buffer = allocate_n uint16_t ~count:(needed + 1) in
  error_code <-@ Int32.zero;
  let actual =
    I.udat_format formatter.ptr value buffer (needed + 1) null_void error_code in
  Icu4c_strings.check_error !@error_code;
  Icu4c_strings.utf16_to_utf8 buffer actual

let format_with_options opts value =
  let formatter = make opts in
  Fun.protect
    ~finally:(fun () -> close formatter)
    (fun () -> format formatter value)
