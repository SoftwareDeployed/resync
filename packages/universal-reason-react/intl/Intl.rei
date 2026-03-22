module NumberFormatter: {
  module Style: {
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

  let make: options => t;
  let format: (t, float) => string;
  let formatToParts: (t, float) => list(part);

  let formatWithOptions: (options, float) => string;
  let formatToPartsWithOptions: (options, float) => list(part);
};

module DateTimeFormatter: {
  module Style: {
    type t =
      | Full
      | Long
      | Medium
      | Short;
  };

  module Text: {
    type t =
      | Narrow
      | Short
      | Long;
  };

  module Numeric: {
    type t =
      | Numeric
      | TwoDigit;
  };

  module Month: {
    type t =
      | Numeric
      | TwoDigit
      | Narrow
      | Short
      | Long;
  };

  module HourCycle: {
    type t =
      | H11
      | H12
      | H23
      | H24;
  };

  module TimeZoneName: {
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

  let make: options => t;
  let format: (t, float) => string;
  let formatToParts: (t, float) => list(part);

  let formatWithOptions: (options, float) => string;
  let formatToPartsWithOptions: (options, float) => list(part);
};
