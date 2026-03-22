/* Number formatting using universal Intl API */

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

  let formatter =
    Intl.NumberFormatter.make({
      locale: Some(locale_),
      style: Some(Intl.NumberFormatter.Style.Currency),
      currency: Some(currency_),
      minimumFractionDigits: None,
      maximumFractionDigits: None,
      useGrouping: None,
    });

  formatter->Intl.NumberFormatter.format(amount);
};
