open Alcobar
open Ocaml_icu4c

let locale_gen =
  choose
    [
      const "en-US";
      const "en-GB";
      const "fr-FR";
      const "de-DE";
      const "ja-JP";
      const "zh-CN";
      const "es-ES";
      const "pt-BR";
      bytes;
    ]

let utf8_roundtrip_test s =
  let buf, len =
    try Icu4c_strings.utf8_to_utf16 s
    with Failure _ -> bad_test ()
  in
  let s' = Icu4c_strings.utf16_to_utf8 buf len in
  check_eq s s'

let number_style_gen =
  choose
    [
      const Icu4c_number.Style.Decimal;
      const Icu4c_number.Style.Currency;
      const Icu4c_number.Style.Percent;
    ]

let number_format_test locale style min_frac max_frac value =
  let opts =
    Icu4c_number.make_options
      ~style
      ~locale
      ?minimum_fraction_digits:(if min_frac < 0 then None else Some min_frac)
      ?maximum_fraction_digits:(if max_frac < 0 then None else Some max_frac)
      ()
  in
  try ignore (Icu4c_number.format_with_options opts value)
  with Failure _ | Invalid_argument _ -> bad_test ()

let date_style_gen =
  choose
    [
      const None;
      const (Some Icu4c_datetime.Style.Short);
      const (Some Icu4c_datetime.Style.Medium);
      const (Some Icu4c_datetime.Style.Long);
      const (Some Icu4c_datetime.Style.Full);
    ]

let datetime_format_test locale date_style time_style value =
  let opts =
    Icu4c_datetime.make_options
      ~locale
      ?date_style
      ?time_style
      ()
  in
  try ignore (Icu4c_datetime.format_with_options opts value)
  with Failure _ | Invalid_argument _ -> bad_test ()

let locale_normalize_test locale =
  try ignore (Icu4c_locale.normalize_locale locale)
  with Invalid_argument _ -> bad_test ()

let suite =
  ( "ICU4cFuzz",
    [
      test_case "utf8 roundtrip" [ bytes ] utf8_roundtrip_test;
      test_case "number format does not crash"
        [ locale_gen; number_style_gen; range 10; range 10; float ]
        number_format_test;
      test_case "datetime format does not crash"
        [ locale_gen; date_style_gen; date_style_gen; float ]
        datetime_format_test;
      test_case "locale normalization does not crash"
        [ bytes ]
        locale_normalize_test;
    ] )

let () = run "icu4c-fuzz" [ suite ]
