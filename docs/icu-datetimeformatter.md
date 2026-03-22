# icu-datetimeformatter

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.

Native ICU-backed date and time formatting for OCaml/Reason server code.

This package is designed as the date/time sibling to `icu-numberformatter` and provides a server-side path for a pragmatic subset of `Intl.DateTimeFormat`.

## What it provides

- Locale-aware date/time formatting through ICU4C
- Support for `dateStyle` / `timeStyle`
- Support for common component options such as `weekday`, `year`, `month`, `day`, `hour`, `minute`, `second`, `fractionalSecondDigits`, `timeZoneName`, `hour12`, and `hourCycle`
- `format_to_parts` for tokenized output similar to `Intl.DateTimeFormat.prototype.formatToParts()`
- Formatter caching keyed by normalized locale and options

The package is named `resync.icu_datetimeformatter` (library `icu_datetimeformatter`) and exposes the `Date_time_formatter` module.

## Quick start

```reason
let timestamp = 1608434596738.0;

let date = Date_time_formatter.format(
  ~locale="en-US",
  ~time_zone="UTC",
  timestamp,
);

let detailed = Date_time_formatter.format(
  ~locale="en-GB",
  ~time_zone="Australia/Sydney",
  ~date_style=Date_time_formatter.Style.Full,
  ~time_style=Date_time_formatter.Style.Long,
  timestamp,
);

let parts = Date_time_formatter.format_to_parts(
  ~locale="en-US",
  ~time_zone="UTC",
  ~year=Date_time_formatter.Numeric.Numeric,
  ~month=Date_time_formatter.Month.Numeric,
  ~day=Date_time_formatter.Numeric.Numeric,
  timestamp,
);
```

## API overview

All formatting functions accept a Unix-epoch timestamp in milliseconds as `float` and return a formatted `string` or `part list`.

```reason
module Style: {
  type t = Full | Long | Medium | Short;
};

module Text: {
  type t = Narrow | Short | Long;
};

module Numeric: {
  type t = Numeric | Two_digit;
};

module Month: {
  type t = Numeric | Two_digit | Narrow | Short | Long;
};

module Hour_cycle: {
  type t = H11 | H12 | H23 | H24;
};

module Time_zone_name: {
  type t =
    | Short
    | Long
    | Short_offset
    | Long_offset
    | Short_generic
    | Long_generic;
};

type part = {
  type_: string,
  value: string,
};

val format:
  ?locale:string ->
  ?time_zone:string ->
  ?date_style:Style.t ->
  ?time_style:Style.t ->
  ?weekday:Text.t ->
  ?era:Text.t ->
  ?year:Numeric.t ->
  ?month:Month.t ->
  ?day:Numeric.t ->
  ?hour:Numeric.t ->
  ?minute:Numeric.t ->
  ?second:Numeric.t ->
  ?fractional_second_digits:int ->
  ?time_zone_name:Time_zone_name.t ->
  ?hour12:bool ->
  ?hour_cycle:Hour_cycle.t ->
  float ->
  string;

val format_to_parts:
  ?locale:string ->
  ?time_zone:string ->
  ?date_style:Style.t ->
  ?time_style:Style.t ->
  ?weekday:Text.t ->
  ?era:Text.t ->
  ?year:Numeric.t ->
  ?month:Month.t ->
  ?day:Numeric.t ->
  ?hour:Numeric.t ->
  ?minute:Numeric.t ->
  ?second:Numeric.t ->
  ?fractional_second_digits:int ->
  ?time_zone_name:Time_zone_name.t ->
  ?hour12:bool ->
  ?hour_cycle:Hour_cycle.t ->
  float ->
  list(part);
```

## Current scope

This package currently aims at the common server-rendering subset of `Intl.DateTimeFormat`:

- `format`
- `formatToParts`
- BCP47 locale input
- explicit `timeZone`
- style-based formatting and common date/time component options

Not implemented yet:

- `resolvedOptions()`
- `supportedLocalesOf()`
- `formatRange()`
- `formatRangeToParts()`

## Configuration and behavior

- ICU symbols are discovered at build time by `packages/icu-datetimeformatter/src/discover.ml`.
- The package uses version-suffixed C bindings in the same style as `icu-numberformatter`.
- UTF conversion is done through ICU conversion helpers rather than ad-hoc ASCII-only conversion.
