(* Native Implementation using ocaml-icu4c *)

module NumberFormatter = struct
  module Style = struct
    type t = Decimal | Currency | Percent
  end

  type part = {
    type_: string;
    value: string;
  }

  type options = {
    locale: string option;
    style: Style.t option;
    currency: string option;
    minimumFractionDigits: int option;
    maximumFractionDigits: int option;
    useGrouping: bool option;
  }

  type t = Ocaml_icu4c.Icu4c_number.t

  let style_to_icu = function
    | Style.Decimal -> Ocaml_icu4c.Icu4c_number.Style.Decimal
    | Style.Currency -> Ocaml_icu4c.Icu4c_number.Style.Currency
    | Style.Percent -> Ocaml_icu4c.Icu4c_number.Style.Percent

  let make opts =
    let locale = match opts.locale with Some l -> l | None -> "en-US" in
    let style = match opts.style with Some s -> Some (style_to_icu s) | None -> None in
    let icu_opts = Ocaml_icu4c.Icu4c_number.make_options
      ~locale
      ?style
      ?currency:opts.currency
      ?minimum_fraction_digits:opts.minimumFractionDigits
      ?maximum_fraction_digits:opts.maximumFractionDigits
      ?use_grouping:opts.useGrouping
      () in
    Ocaml_icu4c.Icu4c_number.make icu_opts

  let format formatter value =
    Ocaml_icu4c.Icu4c_number.format formatter value

  let formatToParts _formatter _value =
    (* TODO: Implement formatToParts using ICU formatted-value APIs *)
    []

  let formatWithOptions opts value =
    let locale = match opts.locale with Some l -> l | None -> "en-US" in
    let style = match opts.style with Some s -> Some (style_to_icu s) | None -> None in
    let icu_opts = Ocaml_icu4c.Icu4c_number.make_options
      ~locale
      ?style
      ?currency:opts.currency
      ?minimum_fraction_digits:opts.minimumFractionDigits
      ?maximum_fraction_digits:opts.maximumFractionDigits
      ?use_grouping:opts.useGrouping
      () in
    Ocaml_icu4c.Icu4c_number.format_with_options icu_opts value

  let formatToPartsWithOptions _opts _value =
    (* TODO: Implement formatToParts using ICU formatted-value APIs *)
    []
end

module DateTimeFormatter = struct
  module Style = struct
    type t = Full | Long | Medium | Short
  end

  module Text = struct
    type t = Narrow | Short | Long
  end

  module Numeric = struct
    type t = Numeric | TwoDigit
  end

  module Month = struct
    type t = Numeric | TwoDigit | Narrow | Short | Long
  end

  module HourCycle = struct
    type t = H11 | H12 | H23 | H24
  end

  module TimeZoneName = struct
    type t =
      | Short
      | Long
      | ShortOffset
      | LongOffset
      | ShortGeneric
      | LongGeneric
  end

  type part = {
    type_: string;
    value: string;
  }

  type options = {
    locale: string option;
    timeZone: string option;
    dateStyle: Style.t option;
    timeStyle: Style.t option;
    weekday: Text.t option;
    era: Text.t option;
    year: Numeric.t option;
    month: Month.t option;
    day: Numeric.t option;
    hour: Numeric.t option;
    minute: Numeric.t option;
    second: Numeric.t option;
    fractionalSecondDigits: int option;
    timeZoneName: TimeZoneName.t option;
    hour12: bool option;
    hourCycle: HourCycle.t option;
  }

  type t = Ocaml_icu4c.Icu4c_datetime.t

  let style_to_icu = function
    | Style.Full -> Ocaml_icu4c.Icu4c_datetime.Style.Full
    | Style.Long -> Ocaml_icu4c.Icu4c_datetime.Style.Long
    | Style.Medium -> Ocaml_icu4c.Icu4c_datetime.Style.Medium
    | Style.Short -> Ocaml_icu4c.Icu4c_datetime.Style.Short

  let text_to_icu = function
    | Text.Narrow -> Ocaml_icu4c.Icu4c_datetime.Text.Narrow
    | Text.Short -> Ocaml_icu4c.Icu4c_datetime.Text.Short
    | Text.Long -> Ocaml_icu4c.Icu4c_datetime.Text.Long

  let numeric_to_icu = function
    | Numeric.Numeric -> Ocaml_icu4c.Icu4c_datetime.Numeric.Numeric
    | Numeric.TwoDigit -> Ocaml_icu4c.Icu4c_datetime.Numeric.Two_digit

  let month_to_icu = function
    | Month.Numeric -> Ocaml_icu4c.Icu4c_datetime.Month.Numeric
    | Month.TwoDigit -> Ocaml_icu4c.Icu4c_datetime.Month.Two_digit
    | Month.Narrow -> Ocaml_icu4c.Icu4c_datetime.Month.Narrow
    | Month.Short -> Ocaml_icu4c.Icu4c_datetime.Month.Short
    | Month.Long -> Ocaml_icu4c.Icu4c_datetime.Month.Long

  let hour_cycle_to_icu = function
    | HourCycle.H11 -> Ocaml_icu4c.Icu4c_datetime.Hour_cycle.H11
    | HourCycle.H12 -> Ocaml_icu4c.Icu4c_datetime.Hour_cycle.H12
    | HourCycle.H23 -> Ocaml_icu4c.Icu4c_datetime.Hour_cycle.H23
    | HourCycle.H24 -> Ocaml_icu4c.Icu4c_datetime.Hour_cycle.H24

  let time_zone_name_to_icu = function
    | TimeZoneName.Short -> Ocaml_icu4c.Icu4c_datetime.Time_zone_name.Short
    | TimeZoneName.Long -> Ocaml_icu4c.Icu4c_datetime.Time_zone_name.Long
    | TimeZoneName.ShortOffset -> Ocaml_icu4c.Icu4c_datetime.Time_zone_name.Short_offset
    | TimeZoneName.LongOffset -> Ocaml_icu4c.Icu4c_datetime.Time_zone_name.Long_offset
    | TimeZoneName.ShortGeneric -> Ocaml_icu4c.Icu4c_datetime.Time_zone_name.Short_generic
    | TimeZoneName.LongGeneric -> Ocaml_icu4c.Icu4c_datetime.Time_zone_name.Long_generic

  let make opts =
    let locale = match opts.locale with Some l -> l | None -> "en-US" in
    let icu_opts = Ocaml_icu4c.Icu4c_datetime.make_options
      ~locale
      ?time_zone:opts.timeZone
      ?date_style:(Option.map style_to_icu opts.dateStyle)
      ?time_style:(Option.map style_to_icu opts.timeStyle)
      ?weekday:(Option.map text_to_icu opts.weekday)
      ?era:(Option.map text_to_icu opts.era)
      ?year:(Option.map numeric_to_icu opts.year)
      ?month:(Option.map month_to_icu opts.month)
      ?day:(Option.map numeric_to_icu opts.day)
      ?hour:(Option.map numeric_to_icu opts.hour)
      ?minute:(Option.map numeric_to_icu opts.minute)
      ?second:(Option.map numeric_to_icu opts.second)
      ?fractional_second_digits:opts.fractionalSecondDigits
      ?time_zone_name:(Option.map time_zone_name_to_icu opts.timeZoneName)
      ?hour12:opts.hour12
      ?hour_cycle:(Option.map hour_cycle_to_icu opts.hourCycle)
      () in
    Ocaml_icu4c.Icu4c_datetime.make icu_opts

  let format formatter value =
    Ocaml_icu4c.Icu4c_datetime.format formatter value

  let formatToParts _formatter _value =
    (* TODO: Implement formatToParts *)
    []

  let formatWithOptions opts value =
    let locale = match opts.locale with Some l -> l | None -> "en-US" in
    let icu_opts = Ocaml_icu4c.Icu4c_datetime.make_options
      ~locale
      ?time_zone:opts.timeZone
      ?date_style:(Option.map style_to_icu opts.dateStyle)
      ?time_style:(Option.map style_to_icu opts.timeStyle)
      ?weekday:(Option.map text_to_icu opts.weekday)
      ?era:(Option.map text_to_icu opts.era)
      ?year:(Option.map numeric_to_icu opts.year)
      ?month:(Option.map month_to_icu opts.month)
      ?day:(Option.map numeric_to_icu opts.day)
      ?hour:(Option.map numeric_to_icu opts.hour)
      ?minute:(Option.map numeric_to_icu opts.minute)
      ?second:(Option.map numeric_to_icu opts.second)
      ?fractional_second_digits:opts.fractionalSecondDigits
      ?time_zone_name:(Option.map time_zone_name_to_icu opts.timeZoneName)
      ?hour12:opts.hour12
      ?hour_cycle:(Option.map hour_cycle_to_icu opts.hourCycle)
      () in
    Ocaml_icu4c.Icu4c_datetime.format_with_options icu_opts value

  let formatToPartsWithOptions _opts _value =
    (* TODO: Implement formatToParts *)
    []
end
