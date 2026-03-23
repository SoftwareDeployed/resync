/* JS Implementation using browser/node Intl API */

module NumberFormatter = {
  module Style = {
    type t =
      | Decimal
      | Currency
      | Percent;
  };

  type part = {
    type_: string,
    value: string,
  };

  type options = {
    locale: option(string),
    style: option(Style.t),
    currency: option(string),
    minimumFractionDigits: option(int),
    maximumFractionDigits: option(int),
    useGrouping: option(bool),
  };

  type t;

  [@mel.new] [@mel.scope "Intl"]
  external makeIntl: (string, Js.t({..})) =>
  t = "NumberFormat";

  [@mel.send]
  external format: (t, float) => string = "format";

  type jsPart = {
    .
    "type": string,
    "value": string,
  };

  [@mel.send]
  external formatToParts: (t, float) => array(jsPart) = "formatToParts";

  let styleToString = style =>
    switch (style) {
    | Style.Decimal => "decimal"
    | Style.Currency => "currency"
    | Style.Percent => "percent"
    };

  let makeWithOptions = (opts: options) => {
    let locale =
      switch (opts.locale) {
      | Some(l) => l
      | None => "en-US"
      };
    let jsOpts = Js.Dict.empty();

    switch (opts.style) {
    | Some(style) => Js.Dict.set(jsOpts, "style", Js.Json.string(styleToString(style)))
    | None => ()
    };

    switch (opts.currency) {
    | Some(currency) => Js.Dict.set(jsOpts, "currency", Js.Json.string(currency))
    | None => ()
    };

    switch (opts.minimumFractionDigits) {
    | Some(digits) => Js.Dict.set(jsOpts, "minimumFractionDigits", Js.Json.number(float_of_int(digits)))
    | None => ()
    };

    switch (opts.maximumFractionDigits) {
    | Some(digits) => Js.Dict.set(jsOpts, "maximumFractionDigits", Js.Json.number(float_of_int(digits)))
    | None => ()
    };

    switch (opts.useGrouping) {
    | Some(grouping) => Js.Dict.set(jsOpts, "useGrouping", Js.Json.boolean(grouping))
    | None => ()
    };

    makeIntl(locale, Obj.magic(jsOpts));
  };

  let make = (
    ~locale=?,
    ~style=?,
    ~currency=?,
    ~minimumFractionDigits=?,
    ~maximumFractionDigits=?,
    ~useGrouping=?,
    (),
  ) =>
    makeWithOptions({
      locale,
      style,
      currency,
      minimumFractionDigits,
      maximumFractionDigits,
      useGrouping,
    });

  let formatToParts = (formatter, value) => {
    let parts = formatToParts(formatter, value);
    Array.to_list(parts)
    |> List.map(part => {
         let typedPart = Obj.magic(part);
         {type_: typedPart##type_, value: part##value};
       });
  };

  let formatWithOptions = (
    ~locale=?,
    ~style=?,
    ~currency=?,
    ~minimumFractionDigits=?,
    ~maximumFractionDigits=?,
    ~useGrouping=?,
    value,
  ) => {
    let formatter =
      make(
        ~locale?,
        ~style?,
        ~currency?,
        ~minimumFractionDigits?,
        ~maximumFractionDigits?,
        ~useGrouping?,
        (),
      );
    format(formatter, value);
  };

  let formatToPartsWithOptions = (
    ~locale=?,
    ~style=?,
    ~currency=?,
    ~minimumFractionDigits=?,
    ~maximumFractionDigits=?,
    ~useGrouping=?,
    value,
  ) => {
    let formatter =
      make(
        ~locale?,
        ~style?,
        ~currency?,
        ~minimumFractionDigits?,
        ~maximumFractionDigits?,
        ~useGrouping?,
        (),
      );
    formatToParts(formatter, value);
  };
};

module DateTimeFormatter = {
  module Style = {
    type t =
      | Full
      | Long
      | Medium
      | Short;
  };

  module Text = {
    type t =
      | Narrow
      | Short
      | Long;
  };

  module Numeric = {
    type t =
      | Numeric
      | TwoDigit;
  };

  module Month = {
    type t =
      | Numeric
      | TwoDigit
      | Narrow
      | Short
      | Long;
  };

  module HourCycle = {
    type t =
      | H11
      | H12
      | H23
      | H24;
  };

  module TimeZoneName = {
    type t =
      | Short
      | Long
      | ShortOffset
      | LongOffset
      | ShortGeneric
      | LongGeneric;
  };

  type part = {
    type_: string,
    value: string,
  };

  type options = {
    locale: option(string),
    timeZone: option(string),
    dateStyle: option(Style.t),
    timeStyle: option(Style.t),
    weekday: option(Text.t),
    era: option(Text.t),
    year: option(Numeric.t),
    month: option(Month.t),
    day: option(Numeric.t),
    hour: option(Numeric.t),
    minute: option(Numeric.t),
    second: option(Numeric.t),
    fractionalSecondDigits: option(int),
    timeZoneName: option(TimeZoneName.t),
    hour12: option(bool),
    hourCycle: option(HourCycle.t),
  };

  type t;

  [@mel.new] [@mel.scope "Intl"]
  external makeIntl: (string, Js.t({..})) =>
  t = "DateTimeFormat";

  [@mel.send]
  external format: (t, float) => string = "format";

  type jsPart = {
    .
    "type": string,
    "value": string,
  };

  [@mel.send]
  external formatToParts: (t, float) => array(jsPart) = "formatToParts";

  let styleToString = style =>
    switch (style) {
    | Style.Full => "full"
    | Style.Long => "long"
    | Style.Medium => "medium"
    | Style.Short => "short"
    };

  let textToString = text =>
    switch (text) {
    | Text.Narrow => "narrow"
    | Text.Short => "short"
    | Text.Long => "long"
    };

  let numericToString = numeric =>
    switch (numeric) {
    | Numeric.Numeric => "numeric"
    | Numeric.TwoDigit => "2-digit"
    };

  let monthToString = month =>
    switch (month) {
    | Month.Numeric => "numeric"
    | Month.TwoDigit => "2-digit"
    | Month.Narrow => "narrow"
    | Month.Short => "short"
    | Month.Long => "long"
    };

  let hourCycleToString = hourCycle =>
    switch (hourCycle) {
    | HourCycle.H11 => "h11"
    | HourCycle.H12 => "h12"
    | HourCycle.H23 => "h23"
    | HourCycle.H24 => "h24"
    };

  let timeZoneNameToString = timeZoneName =>
    switch (timeZoneName) {
    | TimeZoneName.Short => "short"
    | TimeZoneName.Long => "long"
    | TimeZoneName.ShortOffset => "shortOffset"
    | TimeZoneName.LongOffset => "longOffset"
    | TimeZoneName.ShortGeneric => "shortGeneric"
    | TimeZoneName.LongGeneric => "longGeneric"
    };

  let makeWithOptions = (opts: options) => {
    let locale =
      switch (opts.locale) {
      | Some(l) => l
      | None => "en-US"
      };
    let jsOpts = Js.Dict.empty();

    switch (opts.timeZone) {
    | Some(tz) => Js.Dict.set(jsOpts, "timeZone", Js.Json.string(tz))
    | None => ()
    };

    switch (opts.dateStyle) {
    | Some(style) => Js.Dict.set(jsOpts, "dateStyle", Js.Json.string(styleToString(style)))
    | None => ()
    };

    switch (opts.timeStyle) {
    | Some(style) => Js.Dict.set(jsOpts, "timeStyle", Js.Json.string(styleToString(style)))
    | None => ()
    };

    switch (opts.weekday) {
    | Some(text) => Js.Dict.set(jsOpts, "weekday", Js.Json.string(textToString(text)))
    | None => ()
    };

    switch (opts.era) {
    | Some(text) => Js.Dict.set(jsOpts, "era", Js.Json.string(textToString(text)))
    | None => ()
    };

    switch (opts.year) {
    | Some(numeric) => Js.Dict.set(jsOpts, "year", Js.Json.string(numericToString(numeric)))
    | None => ()
    };

    switch (opts.month) {
    | Some(month) => Js.Dict.set(jsOpts, "month", Js.Json.string(monthToString(month)))
    | None => ()
    };

    switch (opts.day) {
    | Some(numeric) => Js.Dict.set(jsOpts, "day", Js.Json.string(numericToString(numeric)))
    | None => ()
    };

    switch (opts.hour) {
    | Some(numeric) => Js.Dict.set(jsOpts, "hour", Js.Json.string(numericToString(numeric)))
    | None => ()
    };

    switch (opts.minute) {
    | Some(numeric) => Js.Dict.set(jsOpts, "minute", Js.Json.string(numericToString(numeric)))
    | None => ()
    };

    switch (opts.second) {
    | Some(numeric) => Js.Dict.set(jsOpts, "second", Js.Json.string(numericToString(numeric)))
    | None => ()
    };

    switch (opts.fractionalSecondDigits) {
    | Some(digits) => Js.Dict.set(jsOpts, "fractionalSecondDigits", Js.Json.number(float_of_int(digits)))
    | None => ()
    };

    switch (opts.timeZoneName) {
    | Some(name) => Js.Dict.set(jsOpts, "timeZoneName", Js.Json.string(timeZoneNameToString(name)))
    | None => ()
    };

    switch (opts.hour12) {
    | Some(hour12) => Js.Dict.set(jsOpts, "hour12", Js.Json.boolean(hour12))
    | None => ()
    };

    switch (opts.hourCycle) {
    | Some(cycle) => Js.Dict.set(jsOpts, "hourCycle", Js.Json.string(hourCycleToString(cycle)))
    | None => ()
    };

    makeIntl(locale, Obj.magic(jsOpts));
  };

  let make = (
    ~locale=?,
    ~timeZone=?,
    ~dateStyle=?,
    ~timeStyle=?,
    ~weekday=?,
    ~era=?,
    ~year=?,
    ~month=?,
    ~day=?,
    ~hour=?,
    ~minute=?,
    ~second=?,
    ~fractionalSecondDigits=?,
    ~timeZoneName=?,
    ~hour12=?,
    ~hourCycle=?,
    (),
  ) =>
    makeWithOptions({
      locale,
      timeZone,
      dateStyle,
      timeStyle,
      weekday,
      era,
      year,
      month,
      day,
      hour,
      minute,
      second,
      fractionalSecondDigits,
      timeZoneName,
      hour12,
      hourCycle,
    });

  let formatToParts = (formatter, value) => {
    let parts = formatToParts(formatter, value);
    Array.to_list(parts)
    |> List.map(part => {
         let typedPart = Obj.magic(part);
         {type_: typedPart##type_, value: part##value};
       });
  };

  let formatWithOptions = (
    ~locale=?,
    ~timeZone=?,
    ~dateStyle=?,
    ~timeStyle=?,
    ~weekday=?,
    ~era=?,
    ~year=?,
    ~month=?,
    ~day=?,
    ~hour=?,
    ~minute=?,
    ~second=?,
    ~fractionalSecondDigits=?,
    ~timeZoneName=?,
    ~hour12=?,
    ~hourCycle=?,
    value,
  ) => {
    let formatter =
      make(
        ~locale?,
        ~timeZone?,
        ~dateStyle?,
        ~timeStyle?,
        ~weekday?,
        ~era?,
        ~year?,
        ~month?,
        ~day?,
        ~hour?,
        ~minute?,
        ~second?,
        ~fractionalSecondDigits?,
        ~timeZoneName?,
        ~hour12?,
        ~hourCycle?,
        (),
      );
    format(formatter, value);
  };

  let formatToPartsWithOptions = (
    ~locale=?,
    ~timeZone=?,
    ~dateStyle=?,
    ~timeStyle=?,
    ~weekday=?,
    ~era=?,
    ~year=?,
    ~month=?,
    ~day=?,
    ~hour=?,
    ~minute=?,
    ~second=?,
    ~fractionalSecondDigits=?,
    ~timeZoneName=?,
    ~hour12=?,
    ~hourCycle=?,
    value,
  ) => {
    let formatter =
      make(
        ~locale?,
        ~timeZone?,
        ~dateStyle?,
        ~timeStyle?,
        ~weekday?,
        ~era?,
        ~year?,
        ~month?,
        ~day?,
        ~hour?,
        ~minute?,
        ~second?,
        ~fractionalSecondDigits?,
        ~timeZoneName?,
        ~hour12?,
        ~hourCycle?,
        (),
      );
    formatToParts(formatter, value);
  };
};
