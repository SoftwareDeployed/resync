type numberFormatter;
type numberFormatOptions;

[@mel.scope "Intl"]
external make: (string, numberFormatOptions) => numberFormatter =
  "NumberFormat";

[@mel.send] external format: (numberFormatter, float) => string = "format";

[@mel.obj]
external makeOptions:
  (~style: string, ~currency: string, unit) => numberFormatOptions;

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
    Number_formatter.format_currency(~locale=locale_, ~currency=currency_, amount)
  | Client =>
    let formatter =
      make(locale_, makeOptions(~style="currency", ~currency=currency_, ()));

    formatter->format(amount)
  };
};
