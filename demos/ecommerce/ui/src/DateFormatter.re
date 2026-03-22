type dateTimeFormatter;
type dateTimeFormatOptions;

[@mel.scope "Intl"]
external make: (string, dateTimeFormatOptions) => dateTimeFormatter =
  "DateTimeFormat";

[@mel.send] external format: (dateTimeFormatter, float) => string = "format";

[@mel.obj]
external makeOptions: (~timeZone: string, unit) => dateTimeFormatOptions;

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

  switch%platform (Runtime.platform) {
  | Server =>
    Date_time_formatter.format(~locale=locale_, ~time_zone="UTC", value)
  | Client =>
    let formatter = make(locale_, makeOptions(~timeZone="UTC", ()));

    formatter->format(value)
  };
};
