# icu-numberformatter

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.

Native ICU-backed number formatting for OCaml/Reason server code.

This package powers ecommerce server-side currency rendering in the demo and avoids the naive string-based formatting path.

## What it provides

- `format_currency` for locale-aware currency output
- `format_decimal` for fixed or dynamic precision decimals
- `format_percent` for percent output with ICU locale rules
- A lightweight formatter cache so ICU formatters are created once per key
- Fail-fast startup behavior if required ICU libraries are missing

The package is named `resync.icu_numberformatter` (library `icu_numberformatter`) and exposes the `Number_formatter` module.

## Quick start

```reason
let usd = Number_formatter.format_currency(1234.56);
let eur = Number_formatter.format_currency(
  ~locale="de-DE",
  ~currency="EUR",
  1234.56,
);
let euros = Number_formatter.format_currency(~currency="EUR", 5.0);
let decimal = Number_formatter.format_decimal(~min_fraction=2, ~max_fraction=2, 12.5);
let percent = Number_formatter.format_percent(0.756);
``` 

## API overview

All functions return formatted `string` values.

```reason
type style = Currency | Decimal | Percent

type options = {
  style: style,
  currency: option(string),
  locale: string,
  minimum_fraction_digits: option(int),
  maximum_fraction_digits: option(int),
  use_grouping: option(bool),
};

val format_currency:
  ?locale:string ->
  ?currency:string ->
  ?min_fraction:int ->
  ?max_fraction:int ->
  float ->
  string;

val format_decimal:
  ?locale:string ->
  ?min_fraction:int ->
  ?max_fraction:int ->
  ?grouping:bool ->
  float ->
  string;

val format_percent:
  ?locale:string ->
  ?min_fraction:int ->
  ?max_fraction:int ->
  float ->
  string;
```

## Server-side usage in ecommerce

The ecommerce UI already uses this module on server rendering for currency formatting:

```reason
/* demos/ecommerce/ui/src/NumberFormatter.re */
| Server => Number_formatter.format_currency(~locale=locale_, ~currency=currency_, amount)
```

On client rendering, the existing `Intl.NumberFormat` path remains in place.

To use it in another server executable, include the dependency in dune:

```lisp
(executable
  ...
  (libraries
   ...
   icu_numberformatter
   ...))
```

## Configuration and behavior

- ICU symbols are discovered at build time by `packages/icu-numberformatter/src/discover.ml`.
- If ICU is missing or discovery fails, the package emits a clear startup error.
- The discover phase writes `c_libs.sexp` and `c_flags.sexp` used by dune.

## Notes

- Versioned ICU symbols are resolved automatically from the local installation headers.
- `format_percent` defaults to integer percent output (for example, `0.756` -> `76%`) to match the current ecommerce UI behavior.
