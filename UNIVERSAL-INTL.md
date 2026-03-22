# Universal Intl Plan

## Goal

Replace the separate native-only `icu-numberformatter` and `icu-datetimeformatter`
packages with a new universal formatting stack:

- `packages/ocaml-icu4c`
  - shared native ICU4C foundation
- `packages/universal-reason-react/intl`
  - JS target with zero-cost bindings to browser/node `Intl`
  - native target backed by `ocaml-icu4c`
- a single public `Intl` API with:
  - `Intl.NumberFormatter`
  - `Intl.DateTimeFormatter`

This is a clean-break migration. The old formatter packages are removed after the
new package is wired up.

## Desired Outcome

From either Melange JS or native code, the call shape is the same:

```reason
let numberFormatter =
  Intl.NumberFormatter.make({
    locale: Some("en-US"),
    style: Some(Intl.NumberFormatter.Style.Currency),
    currency: Some("USD"),
    minimumFractionDigits: None,
    maximumFractionDigits: None,
    useGrouping: None,
  });

let price = numberFormatter->Intl.NumberFormatter.format(1234.56);

let dateFormatter =
  Intl.DateTimeFormatter.make({
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

let date = dateFormatter->Intl.DateTimeFormatter.format(1608434596738.0);
```

The JS target should compile to thin `Intl.NumberFormat` /
`Intl.DateTimeFormat` bindings with no runtime platform branching. The native
target should use ICU4C through shared low-level bindings.

## Package Layout

### 1. Native ICU foundation

Create:

```text
packages/ocaml-icu4c/
  dune
  src/
    dune
    discover.ml
    icu4c_config.ml        (generated)
    Icu4c.ml
    Icu4c.mli
    Icu4c_strings.ml
    Icu4c_strings.mli
    Icu4c_locale.ml
    Icu4c_locale.mli
    Icu4c_number.ml
    Icu4c_number.mli
    Icu4c_datetime.ml
    Icu4c_datetime.mli
```

Recommended dune identity:

- `name`: `ocaml_icu4c`
- `public_name`: `resync.ocaml_icu4c`

This package owns:

- ICU discovery/configuration logic currently duplicated in the formatter packages
- version-suffixed symbol handling
- UTF-8 / UTF-16 conversion helpers
- locale and time-zone normalization helpers
- raw number/date ICU bindings

This package should be wrapped to avoid more module collisions.

### 2. Universal Intl package

Create:

```text
packages/universal-reason-react/intl/
  dune
  package.json
  js/
    dune
    Intl.re
    Intl.rei
  native/
    dune
    Intl.re
    Intl.rei
```

Recommended dune identity:

- JS library
  - `name`: `universal_reason_react_intl_js`
  - `public_name`: `resync.universal_reason_react_intl_js`
- native library
  - `name`: `universal_reason_react_intl_native`
  - `public_name`: `resync.universal_reason_react_intl_native`
- npm package name
  - `@universal-reason-react/intl`

This matches the repo's existing `universal-reason-react/*` split-target layout.

## Public API Shape

The public boundary should use JS/Intl-style naming so JS and native feel
identical.

## Top-level module

Expose a single `Intl` module with two submodules:

- `Intl.NumberFormatter`
- `Intl.DateTimeFormatter`

## NumberFormatter

Target shape:

```reason
module Intl = {
  module NumberFormatter = {
    module Style = {
      type t = Decimal | Currency | Percent;
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
    let format: t => float => string;
    let formatToParts: t => float => list(part);

    let formatWithOptions: options => float => string;
    let formatToPartsWithOptions: options => float => list(part);
  };
};
```

Notes:

- Native implementation must add `formatToParts`, which does not exist in the
  current `Number_formatter` package.
- Keep part shape aligned with JS `Intl.NumberFormat#formatToParts`, using
  `type_` as the Reason-safe field name.

## DateTimeFormatter

Target shape:

```reason
module Intl = {
  module DateTimeFormatter = {
    module Style = {
      type t = Full | Long | Medium | Short;
    };

    module Text = {
      type t = Narrow | Short | Long;
    };

    module Numeric = {
      type t = Numeric | Two_digit;
    };

    module Month = {
      type t = Numeric | Two_digit | Narrow | Short | Long;
    };

    module HourCycle = {
      type t = H11 | H12 | H23 | H24;
    };

    module TimeZoneName = {
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
    let format: t => float => string;
    let formatToParts: t => float => list(part);

    let formatWithOptions: options => float => string;
    let formatToPartsWithOptions: options => float => list(part);
  };
};
```

Notes:

- The universal input remains `float` epoch milliseconds.
- Date-only normalization stays outside this API.

## JS Target Strategy

The JS target is a thin wrapper over browser/node `Intl`.

### NumberFormatter JS binding

- bind `Intl.NumberFormat`
- `type t` should be the raw JS formatter object
- `make` should compile to `new Intl.NumberFormat(locale, options)`
- `format` should compile to `.format(value)`
- `formatToParts` should compile to `.formatToParts(value)`

### DateTimeFormatter JS binding

- bind `Intl.DateTimeFormat`
- `make` should compile to `new Intl.DateTimeFormat(locale, options)`
- `format` should compile to `.format(value)`
- `formatToParts` should compile to `.formatToParts(value)`

### Important JS rules

- no `switch%platform` inside the universal package
- no fallback adapter layer on JS
- no app-specific date normalization inside the core formatter API
- keep the generated JS close to hand-written `Intl` calls

## Native Target Strategy

The native target is a high-level wrapper over `ocaml-icu4c`.

### Native NumberFormatter work

Port logic from current `packages/icu-numberformatter`, but update it to the new
API shape:

- constructor-style `make`
- instance-based `format`
- `formatWithOptions`
- `formatToParts`

Native `formatToParts` for number formatting requires additional ICU work using
formatted-value APIs. Likely bindings needed:

- `unumf_resultAsValue`
- `ufmtval_nextPosition`
- `ucfpos_open`
- `ucfpos_close`
- `ucfpos_constrainCategory` / `ucfpos_constrainField` as needed

### Native DateTimeFormatter work

Port current `packages/icu-datetimeformatter` logic into the new API shape:

- move option names to camelCase at the public layer
- keep native normalization and caching internals
- preserve style/component validation rules

## Shared Implementation Rules

- The JS and native `Intl` modules must export the same `.rei` interface.
- If a feature cannot exist on one side, it should not ship publicly yet.
- Use JS-facing naming at the universal API boundary.
- Keep parity tests for both formatting and `formatToParts`.

## Migration Plan

### Phase 1: Extract `ocaml-icu4c`

1. Create `packages/ocaml-icu4c`.
2. Move duplicated ICU discovery logic out of the formatter packages.
3. Move UTF conversion helpers into shared modules.
4. Move raw bindings for number/date formatting into namespaced submodules.
5. Build and test the new foundation package on native.

### Phase 2: Create `universal-reason-react/intl`

1. Add `packages/universal-reason-react/intl/dune` with `js` / `native` dirs.
2. Add `package.json` named `@universal-reason-react/intl`.
3. Define `Intl.rei` first so both targets conform to the same API.
4. Implement the JS target using Melange `Intl` bindings.
5. Implement the native target using `ocaml-icu4c`.

### Phase 3: Migrate call sites

1. Update ecommerce demo wrappers to use the new package.
2. Replace direct uses of `Number_formatter` / `Date_time_formatter`.
3. Update server and client dune dependencies:
   - JS depends on `resync.universal_reason_react_intl_js`
   - native depends on `resync.universal_reason_react_intl_native`

### Phase 4: Remove old formatter packages

1. Delete `packages/icu-numberformatter`.
2. Delete `packages/icu-datetimeformatter`.
3. Remove old references from docs and demo code.
4. Update package docs to point to `universal-reason-react/intl` and
   `ocaml-icu4c`.

## Documentation Updates Needed

Update:

- `docs/README.md`
- `docs/API_REFERENCE.md`
- package-specific docs for the new `intl` package
- package-specific docs for `ocaml-icu4c`
- any ecommerce demo docs that mention `icu-numberformatter` /
  `icu-datetimeformatter`

## Testing Plan

### Native tests

- unit tests for number formatting
- unit tests for number `formatToParts`
- unit tests for date formatting
- unit tests for date `formatToParts`
- locale normalization tests
- time-zone normalization tests

### JS tests

- parity-focused tests that run against JS `Intl`
- `format` and `formatToParts` smoke tests in Node

### Cross-target parity tests

- compare JS and native outputs for a supported subset:
  - `en-US` currency/decimal/percent
  - dateStyle/timeStyle combinations
  - common date component combinations
  - `formatToParts` part ordering and part types

### App validation

- rebuild ecommerce demo client bundle
- rebuild ecommerce server
- verify SSR/client hydration still matches
- verify date and number formatting wrappers remain stable

## Known Risks

### 1. ICU vs JS `Intl` behavior drift

Not every ICU behavior maps perfectly to browser `Intl`. The universal package
should initially expose only the subset that is stable across both targets.

### 2. Number `formatToParts` native gap

This is the main missing feature compared with the JS side and requires new ICU
bindings work.

### 3. Naming migration

Current native formatter APIs are snake_case. The universal package should move
to camelCase for consistency, which means a breaking API change.

### 4. Module collisions

The old unwrapped ICU packages already produced collisions. `ocaml-icu4c`
should be wrapped and namespaced from the start.

## Acceptance Criteria

This work is complete when:

- `packages/ocaml-icu4c` exists and is the only native ICU binding package
- `packages/universal-reason-react/intl` exists with JS and native targets
- JS and native export the same `Intl` API
- `Intl.NumberFormatter` and `Intl.DateTimeFormatter` both support:
  - `make`
  - `format`
  - `formatToParts`
- ecommerce demo uses the universal package instead of direct JS/native wrappers
- old formatter packages are removed
- docs are updated
- targeted build and parity tests pass

## Recommended Execution Order

1. Build `ocaml-icu4c` first.
2. Move native datetime logic over.
3. Move native number logic over and add `formatToParts`.
4. Add JS universal bindings.
5. Lock the shared `.rei` surface.
6. Migrate ecommerce demo.
7. Remove old packages.
8. Update docs and examples.
