let sample = 1608434596738.

let part type_ value =
  Date_time_formatter.{ type_; value }

let check_parts label expected actual =
  let show_part Date_time_formatter.{ type_; value } =
    type_ ^ ":" ^ value in
  let pp formatter parts =
    Format.pp_print_list
      ~pp_sep:(fun formatter () -> Format.fprintf formatter "; ")
      (fun formatter item -> Format.pp_print_string formatter (show_part item))
      formatter
      parts in
  let equal left right =
    List.length left = List.length right
    && List.for_all2 (fun a b -> a = b) left right in
  Alcotest.check (Alcotest.testable pp equal) label expected actual

let test_default_format () =
  let result = Date_time_formatter.format ~locale:"en-US" ~time_zone:"UTC" sample in
  Alcotest.(check string) "default date" "12/20/2020" result

let test_style_format () =
  let result =
    Date_time_formatter.format
      ~locale:"en-GB"
      ~time_zone:"Australia/Sydney"
      ~date_style:Date_time_formatter.Style.Full
      ~time_style:Date_time_formatter.Style.Long
      sample in
  Alcotest.(check string) "style format" "Sunday, 20 December 2020 at 14:23:16 GMT+11" result

let test_component_format () =
  let result =
    Date_time_formatter.format
      ~locale:"en-US"
      ~time_zone:"UTC"
      ~year:Date_time_formatter.Numeric.Numeric
      ~month:Date_time_formatter.Month.Long
      ~day:Date_time_formatter.Numeric.Numeric
      ~weekday:Date_time_formatter.Text.Long
      sample in
  Alcotest.(check string) "component format" "Sunday, December 20, 2020" result

let test_hour_cycle_format () =
  let result =
    Date_time_formatter.format
      ~locale:"en-US"
      ~time_zone:"UTC"
      ~hour:Date_time_formatter.Numeric.Numeric
      ~minute:Date_time_formatter.Numeric.Two_digit
      ~hour12:false
      sample in
  Alcotest.(check string) "24 hour format" "03:23" result

let test_fractional_seconds () =
  let result =
    Date_time_formatter.format
      ~locale:"en-US"
      ~time_zone:"UTC"
      ~hour:Date_time_formatter.Numeric.Numeric
      ~minute:Date_time_formatter.Numeric.Two_digit
      ~second:Date_time_formatter.Numeric.Two_digit
      ~fractional_second_digits:3
      sample in
  Alcotest.(check string) "fractional seconds" "3:23:16.738\226\128\175AM" result

let test_format_to_parts () =
  let result =
    Date_time_formatter.format_to_parts
      ~locale:"en-US"
      ~time_zone:"UTC"
      ~year:Date_time_formatter.Numeric.Numeric
      ~month:Date_time_formatter.Month.Numeric
      ~day:Date_time_formatter.Numeric.Numeric
      sample in
  let expected =
    [ part "month" "12"
    ; part "literal" "/"
    ; part "day" "20"
    ; part "literal" "/"
    ; part "year" "2020" ] in
  check_parts "format to parts" expected result

let test_time_zone_name () =
  let result =
    Date_time_formatter.format
      ~locale:"en-US"
      ~time_zone:"UTC"
      ~hour:Date_time_formatter.Numeric.Numeric
      ~minute:Date_time_formatter.Numeric.Two_digit
      ~time_zone_name:Date_time_formatter.Time_zone_name.Short
      sample in
  Alcotest.(check string) "time zone name" "3:23\226\128\175AM UTC" result

let () =
  let open Alcotest in
  run "ICU DateTimeFormatter"
    [ ("format", [
          test_case "default" `Quick test_default_format;
          test_case "style" `Quick test_style_format;
          test_case "components" `Quick test_component_format;
          test_case "hour cycle" `Quick test_hour_cycle_format;
          test_case "fractional seconds" `Quick test_fractional_seconds;
          test_case "time zone name" `Quick test_time_zone_name;
        ]);
      ("parts", [
          test_case "date parts" `Quick test_format_to_parts;
        ]);
    ]
