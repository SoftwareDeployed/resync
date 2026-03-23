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

  type t;

  let make: (
    ~locale: string=?,
    ~style: Style.t=?,
    ~currency: string=?,
    ~minimumFractionDigits: int=?,
    ~maximumFractionDigits: int=?,
    ~useGrouping: bool=?,
    unit,
  ) => t;
  let format: (t, float) => string;
  let formatToParts: (t, float) => list(part);

  let formatWithOptions: (
    ~locale: string=?,
    ~style: Style.t=?,
    ~currency: string=?,
    ~minimumFractionDigits: int=?,
    ~maximumFractionDigits: int=?,
    ~useGrouping: bool=?,
    float,
  ) => string;
  let formatToPartsWithOptions: (
    ~locale: string=?,
    ~style: Style.t=?,
    ~currency: string=?,
    ~minimumFractionDigits: int=?,
    ~maximumFractionDigits: int=?,
    ~useGrouping: bool=?,
    float,
  ) => list(part);
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

  type t;

  let make: (
    ~locale: string=?,
    ~timeZone: string=?,
    ~dateStyle: Style.t=?,
    ~timeStyle: Style.t=?,
    ~weekday: Text.t=?,
    ~era: Text.t=?,
    ~year: Numeric.t=?,
    ~month: Month.t=?,
    ~day: Numeric.t=?,
    ~hour: Numeric.t=?,
    ~minute: Numeric.t=?,
    ~second: Numeric.t=?,
    ~fractionalSecondDigits: int=?,
    ~timeZoneName: TimeZoneName.t=?,
    ~hour12: bool=?,
    ~hourCycle: HourCycle.t=?,
    unit,
  ) => t;
  let format: (t, float) => string;
  let formatToParts: (t, float) => list(part);

  let formatWithOptions: (
    ~locale: string=?,
    ~timeZone: string=?,
    ~dateStyle: Style.t=?,
    ~timeStyle: Style.t=?,
    ~weekday: Text.t=?,
    ~era: Text.t=?,
    ~year: Numeric.t=?,
    ~month: Month.t=?,
    ~day: Numeric.t=?,
    ~hour: Numeric.t=?,
    ~minute: Numeric.t=?,
    ~second: Numeric.t=?,
    ~fractionalSecondDigits: int=?,
    ~timeZoneName: TimeZoneName.t=?,
    ~hour12: bool=?,
    ~hourCycle: HourCycle.t=?,
    float,
  ) => string;
  let formatToPartsWithOptions: (
    ~locale: string=?,
    ~timeZone: string=?,
    ~dateStyle: Style.t=?,
    ~timeStyle: Style.t=?,
    ~weekday: Text.t=?,
    ~era: Text.t=?,
    ~year: Numeric.t=?,
    ~month: Month.t=?,
    ~day: Numeric.t=?,
    ~hour: Numeric.t=?,
    ~minute: Numeric.t=?,
    ~second: Numeric.t=?,
    ~fractionalSecondDigits: int=?,
    ~timeZoneName: TimeZoneName.t=?,
    ~hour12: bool=?,
    ~hourCycle: HourCycle.t=?,
    float,
  ) => list(part);
};
