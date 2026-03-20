let test_currency_usd () =
  let result = Number_formatter.format_currency ~currency:"USD" 1234.56 in
  Alcotest.(check string) "USD currency" "$1,234.56" result

let test_currency_eur_de () =
  let result = Number_formatter.format_currency
    ~locale:"de-DE"
    ~currency:"EUR"
    1234.56 in
   Alcotest.(check string) "EUR currency German" "1.234,56\194\160\226\130\172" result

let test_currency_jpy () =
  let result = Number_formatter.format_currency
    ~locale:"ja-JP"
    ~currency:"JPY"
    1234.56 in
   Alcotest.(check string) "JPY currency" "\239\191\1651,235" result

let test_decimal_grouping () =
  let result = Number_formatter.format_decimal 1234567.89 in
  Alcotest.(check string) "decimal with grouping" "1,234,567.89" result

let test_decimal_no_grouping () =
  let result = Number_formatter.format_decimal ~grouping:false 1234567.89 in
  Alcotest.(check string) "decimal no grouping" "1234567.89" result

let test_decimal_fractions () =
  let result = Number_formatter.format_decimal
    ~min_fraction:4
    ~max_fraction:4
    123.5 in
  Alcotest.(check string) "decimal 4 fractions" "123.5000" result

let test_percent () =
  let result = Number_formatter.format_percent 0.756 in
   Alcotest.(check string) "percent" "76%" result

let test_negative () =
  let result = Number_formatter.format_currency ~currency:"USD" (-1234.56) in
  Alcotest.(check string) "negative currency" "-$1,234.56" result

let test_zero () =
  let result = Number_formatter.format_currency ~currency:"USD" 0.0 in
  Alcotest.(check string) "zero currency" "$0.00" result

let () =
  let open Alcotest in
  run "ICU NumberFormatter"
    [ ("currency", [
        test_case "USD" `Quick test_currency_usd;
        test_case "EUR DE" `Quick test_currency_eur_de;
        test_case "JPY" `Quick test_currency_jpy;
        test_case "negative" `Quick test_negative;
        test_case "zero" `Quick test_zero;
      ]);
      ("decimal", [
        test_case "grouping" `Quick test_decimal_grouping;
        test_case "no grouping" `Quick test_decimal_no_grouping;
        test_case "fractions" `Quick test_decimal_fractions;
      ]);
      ("percent", [
        test_case "basic" `Quick test_percent;
      ]);
    ]
