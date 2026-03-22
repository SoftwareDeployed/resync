# ocaml-icu4c

OCaml bindings for ICU4C (International Components for Unicode).

## Overview

This package provides low-level OCaml bindings for ICU4C, the widely-used C/C++ library for Unicode support and internationalization. It serves as the foundation for the `universal-reason-react/intl` package on native targets.

## Installation

```dune
(libraries
  resync.ocaml_icu4c)
```

## Modules

### Icu4c_bindings

Raw C bindings with automatic version suffix handling. All ICU functions are automatically suffixed with the correct version (e.g., `_78` for ICU 78).

### Icu4c_strings

UTF-8 / UTF-16 string conversion utilities.

```ocaml
(* Convert UTF-8 to UTF-16 *)
val utf8_to_utf16 : string -> Unsigned.UInt16.t Ctypes.ptr * int

(* Convert UTF-16 to UTF-8 *)
val utf16_to_utf8 : Unsigned.UInt16.t Ctypes.ptr -> int -> string

(* Convert UTF-16 slice to UTF-8 *)
val utf16_slice_to_utf8 : Unsigned.UInt16.t Ctypes.ptr -> int -> int -> string
```

### Icu4c_locale

Locale and timezone normalization.

```ocaml
(* Normalize a locale string *)
val normalize_locale : string -> string

(* Normalize a timezone string *)
val normalize_time_zone : string -> string
```

### Icu4c_number

Number formatting using ICU4C.

```ocaml
module Style : sig
  type t = Decimal | Currency | Percent
end

type options = {
  style : Style.t;
  currency : string option;
  locale : string;
  minimum_fraction_digits : int option;
  maximum_fraction_digits : int option;
  use_grouping : bool option;
}

type t

val make_options :
  ?style:Style.t ->
  ?currency:string -
  ?locale:string -
  ?minimum_fraction_digits:int -
  ?maximum_fraction_digits:int -
  ?use_grouping:bool -
  unit -> options

val make : options -> t
val format : t -> float -> string
val format_with_options : options -> float -> string
val close : t -> unit
```

### Icu4c_datetime

DateTime formatting using ICU4C.

```ocaml
module Style : sig type t = Full | Long | Medium | Short end
module Text : sig type t = Narrow | Short | Long end
module Numeric : sig type t = Numeric | Two_digit end
module Month : sig type t = Numeric | Two_digit | Narrow | Short | Long end
module Hour_cycle : sig type t = H11 | H12 | H23 | H24 end
module Time_zone_name : sig
  type t =
    | Short
    | Long
    | Short_offset
    | Long_offset
    | Short_generic
    | Long_generic
end

type options = {
  locale : string;
  time_zone : string option;
  date_style : Style.t option;
  time_style : Style.t option;
  weekday : Text.t option;
  era : Text.t option;
  year : Numeric.t option;
  month : Month.t option;
  day : Numeric.t option;
  hour : Numeric.t option;
  minute : Numeric.t option;
  second : Numeric.t option;
  fractional_second_digits : int option;
  time_zone_name : Time_zone_name.t option;
  hour12 : bool option;
  hour_cycle : Hour_cycle.t option;
}

type t

val make_options :
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
  unit -> options

val make : options -> t
val format : t -> float -> string
val format_with_options : options -> float -> string
val close : t -> unit
```

## Build Configuration

The package uses dune-configurator to detect ICU installation:

1. **pkg-config** - Tries to find ICU via pkg-config
2. **Fallback paths** - Checks common installation locations:
   - `/opt/homebrew/Cellar/icu4c@78/78.2`
   - `/opt/homebrew/opt/icu4c`
   - `/usr/local/opt/icu4c`
   - `/usr`

You can override detection by setting:
- `ICU_PREFIX` - Custom ICU installation prefix
- `PKG_CONFIG_PATH` - Custom pkg-config path

## Version Suffix Handling

ICU C functions are versioned (e.g., `unumf_openForSkeletonAndLocale_78`). The package automatically:
1. Detects the ICU version at build time
2. Generates the correct suffix (e.g., `_78`)
3. Applies it to all function bindings

## Usage Notes

- This is a **low-level package** - most users should use `universal-reason-react/intl` instead
- Formatters must be closed with `close` to free native resources
- UTF-16 conversion utilities handle the ICU string format requirements
- Formatters are cached internally for reuse

## Dependencies

- `ctypes` - C FFI bindings
- `ctypes.foreign` - Dynamic function binding
- `integers` - Integer types

## See Also

- [universal-reason-react/intl](universal-reason-react.intl.md) - High-level universal API built on this package
- [ICU Documentation](https://unicode-org.github.io/icu/userguide/)
