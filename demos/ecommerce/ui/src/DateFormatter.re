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
    Intl.DateTimeFormatter.make(
      ~locale=locale_,
      ~timeZone="UTC",
      ~dateStyle=Intl.DateTimeFormatter.Style.Short,
      (),
    );

  formatter->Intl.DateTimeFormatter.format(value);
};
