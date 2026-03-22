/* Date formatting using universal Intl API */

let normalizedDateValue = (date: Js.Date.t) =>
  Js.Date.utc(
    ~year=Js.Date.getFullYear(date),
    ~month=Js.Date.getMonth(date),
    ~date=Js.Date.getDate(date),
    ~hours=12.0,
    (),
  );

let formatDate = (~locale: option(string)=?, date: Js.Date.t) => {
  let locale_ =
    switch (locale) {
    | Some(locale) => locale
    | None => "en-US"
    };
  let value = normalizedDateValue(date);

  let formatter =
    Intl.DateTimeFormatter.make({
      locale: Some(locale_),
      timeZone: Some("UTC"),
      dateStyle: Some(Intl.DateTimeFormatter.Style.Short),
      timeStyle: None,
      weekday: None,
      era: None,
      year: None,
      month: None,
      day: None,
      hour: None,
      minute: None,
      second: None,
      fractionalSecondDigits: None,
      timeZoneName: None,
      hour12: None,
      hourCycle: None,
    });

  formatter->Intl.DateTimeFormatter.format(value);
};
