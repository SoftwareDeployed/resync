/* Date helpers shared by Landing, InventoryList, and Cart (universal). */

let today = () =>
  Js.Date.make()
  |> Js.Date.setHours(~hours=0.0, ~minutes=0.0, ~seconds=0.0, ~milliseconds=0.0)
  |> Js.Date.fromFloat;

let addDays = (date, days) =>
  Js.Date.make(
    ~year=Js.Date.getFullYear(date),
    ~month=Js.Date.getMonth(date),
    ~date=Js.Date.getDate(date) +. days,
    ~hours=0.0,
    ~minutes=0.0,
    ~seconds=0.0,
    (),
  );

let pad2 = value => value < 10 ? "0" ++ Int.to_string(value) : Int.to_string(value);

let isoDate = date => {
  let year = Js.Date.getFullYear(date) |> int_of_float;
  let month = Js.Date.getMonth(date) |> int_of_float |> (value => value + 1);
  let day = Js.Date.getDate(date) |> int_of_float;

  Int.to_string(year) ++ "-" ++ pad2(month) ++ "-" ++ pad2(day);
};

let dateFromIso = value =>
  switch (value |> String.split_on_char('-')) {
  | [year, month, day] =>
    switch (int_of_string_opt(year), int_of_string_opt(month), int_of_string_opt(day)) {
    | (Some(year), Some(month), Some(day)) =>
      Some(
        Js.Date.make(
          ~year=float_of_int(year),
          ~month=float_of_int(month - 1),
          ~date=float_of_int(day),
          ~hours=0.0,
          ~minutes=0.0,
          ~seconds=0.0,
          (),
        ),
      )
    | _ => None
    }
  | _ => None
  };

/* Default to today/tomorrow, honor ?start/&end=, collapse reversed ranges. */
let rangeFromSearchParams = (~today, ~tomorrow, searchParams) => {
  let startParam =
    switch (UniversalRouter.SearchParams.get("start", searchParams)) {
    | Some(value) => dateFromIso(value)
    | None => None
    };
  let endParam =
    switch (UniversalRouter.SearchParams.get("end", searchParams)) {
    | Some(value) => dateFromIso(value)
    | None => None
    };

  let startDate =
    switch (startParam, endParam) {
    | (Some(date), _) => date
    | (None, Some(date)) => date
    | (None, None) => today
    };
  let endDate =
    switch (endParam, startParam) {
    | (Some(date), _) => date
    | (None, Some(date)) => date
    | (None, None) => tomorrow
    };

  Float.compare(Js.Date.getTime(endDate), Js.Date.getTime(startDate)) < 0
    ? (startDate, startDate) : (startDate, endDate);
};

/* The canonical ?start=&end= param set for a date range. */
let searchParamsForRange = (~startDate, ~endDate) =>
  UniversalRouter.SearchParams.empty
  |> (params => UniversalRouter.SearchParams.set(params, "start", isoDate(startDate)))
  |> (params => UniversalRouter.SearchParams.set(params, "end", isoDate(endDate)));

let hrefWithDates = (~pathname, ~startDate, ~endDate) =>
  pathname
  ++ (
    searchParamsForRange(~startDate, ~endDate)
    |> UniversalRouter.SearchParams.toSearch
  );
