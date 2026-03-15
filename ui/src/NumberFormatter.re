type numberFormatter;
type numberFormatOptions;

[@mel.scope "Intl"]
external make: (string, numberFormatOptions) => numberFormatter =
  "NumberFormat";

[@mel.send] external format: (numberFormatter, float) => string = "format";

[@mel.obj]
external makeOptions:
  (~style: string, ~currency: string, unit) => numberFormatOptions;

let rec addThousandsSeparators = digits => {
  let len = String.length(digits);
  if (len <= 3) {
    digits;
  } else {
    addThousandsSeparators(String.sub(digits, 0, len - 3))
    ++ ","
    ++ String.sub(digits, len - 3, 3);
  };
};

let formatCurrency =
    (~locale: option(string)=?, ~currency: option(string)=?, amount: float) => {
  let locale_ =
    switch (locale) {
    | Some(locale) => locale
    | None => "en-US"
    };
  let currency_ =
    switch (currency) {
    | Some(currency) => currency
    | None => "USD"
    };

  switch%platform (Runtime.platform) {
  | Server =>
    let symbol =
      switch (currency_) {
      | "USD" => "$"
      | _ => currency_ ++ " "
      };
    let cents_total = amount *. 100.0;
    let is_negative = cents_total < 0.0;
    let abs_cents = int_of_float(abs_float(cents_total) +. 0.5);
    let dollars = abs_cents / 100;
    let cents = abs_cents mod 100;
    let prefix = is_negative ? "-" ++ symbol : symbol;
    prefix
    ++ addThousandsSeparators(string_of_int(dollars))
    ++ "."
    ++ Printf.sprintf("%02d", cents)
  | Client =>
    let formatter =
      make(locale_, makeOptions(~style="currency", ~currency=currency_, ()));

    formatter->format(amount)
  };
};
