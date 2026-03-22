# universal-reason-react/intl

Universal internationalization library for Reason React applications.

## Overview

This package provides a unified `Intl` API that works identically on both JavaScript (client) and native OCaml (server) targets:

- **`Intl.NumberFormatter`** - Format numbers as currency, decimals, or percentages
- **`Intl.DateTimeFormatter`** - Format dates and times with locale-aware styling

## Installation

The package is available as two targets:

```dune
; For JavaScript/Melange targets
(libraries
  resync.universal_reason_react_intl_js)

; For native/OCaml targets
(libraries
  resync.universal_reason_react_intl_native)
```

## Quick Start

### Number Formatting

```reason
let formatter = Intl.NumberFormatter.make({
  locale: Some("en-US"),
  style: Some(Intl.NumberFormatter.Style.Currency),
  currency: Some("USD"),
  minimumFractionDigits: None,
  maximumFractionDigits: None,
  useGrouping: None,
});

let price = formatter->Intl.NumberFormatter.format(1234.56);
// "$1,234.56"
```

### Date/Time Formatting

```reason
let formatter = Intl.DateTimeFormatter.make({
  locale: Some("en-US"),
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

let date = formatter->Intl.DateTimeFormatter.format(1608434596738.0);
// "12/20/2020"
```

## API Reference

### Intl.NumberFormatter

#### Types

```reason
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
```

#### Functions

- `make: options => t` - Create a formatter with the specified options
- `format: (t, float) => string` - Format a number
- `formatToParts: (t, float) => list(part)` - Format a number into parts
- `formatWithOptions: (options, float) => string` - One-shot format
- `formatToPartsWithOptions: (options, float) => list(part)` - One-shot format to parts

### Intl.DateTimeFormatter

#### Types

```reason
module Style: {
  type t = Full | Long | Medium | Short;
};

module Text: {
  type t = Narrow | Short | Long;
};

module Numeric: {
  type t = Numeric | TwoDigit;
};

module Month: {
  type t = Numeric | TwoDigit | Narrow | Short | Long;
};

module HourCycle: {
  type t = H11 | H12 | H23 | H24;
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
```

#### Functions

- `make: options => t` - Create a formatter with the specified options
- `format: (t, float) => string` - Format a timestamp (milliseconds)
- `formatToParts: (t, float) => list(part)` - Format into parts
- `formatWithOptions: (options, float) => string` - One-shot format
- `formatToPartsWithOptions: (options, float) => list(part)` - One-shot format to parts

## Platform Notes

### JavaScript Target

- Uses browser/Node.js native `Intl.NumberFormat` and `Intl.DateTimeFormat`
- Zero runtime overhead - thin bindings only
- Full feature parity with browser implementation

### Native Target

- Uses ICU4C via the `ocaml-icu4c` package
- Formatters are cached in-process for performance
- Locale and timezone normalization handled automatically

## Migration from Old Packages

This package replaces:
- `resync.icu_numberformatter` → Use `Intl.NumberFormatter`
- `resync.icu_datetimeformatter` → Use `Intl.DateTimeFormatter`

Key changes:
1. **Unified API** - Same code works on both platforms
2. **CamelCase** - Field names use camelCase (e.g., `minimumFractionDigits`)
3. **No platform switching** - Remove `switch%platform` blocks
4. **Options as records** - Pass options as records instead of labeled arguments
