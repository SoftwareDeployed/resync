open Tilia.React;

let str = React.string;

let startOfDay = date =>
  date
  |> Js.Date.setHours(
       ~hours=0.0,
       ~minutes=0.0,
       ~seconds=0.0,
       ~milliseconds=0.0,
     )
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

let pad2 = value =>
  value < 10 ? "0" ++ Int.to_string(value) : Int.to_string(value);

let isoDate = date => {
  let year = Js.Date.getFullYear(date) |> int_of_float;
  let month = Js.Date.getMonth(date) |> int_of_float |> (value => value + 1);
  let day = Js.Date.getDate(date) |> int_of_float;

  Int.to_string(year) ++ "-" ++ pad2(month) ++ "-" ++ pad2(day);
};

let dateFromIso = value =>
  switch (value |> String.split_on_char('-')) {
  | [year, month, day] =>
    switch (
      int_of_string_opt(year),
      int_of_string_opt(month),
      int_of_string_opt(day),
    ) {
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

let dateRangeFromSearchParams = (~today, ~tomorrow, searchParams) => {
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

let hrefWithDates = (~pathname, ~startDate, ~endDate) => {
  let searchParams =
    UniversalRouter.SearchParams.empty
    |> (
      searchParams =>
        UniversalRouter.SearchParams.set(
          searchParams,
          "start",
          isoDate(startDate),
        )
    )
    |> (
      searchParams =>
        UniversalRouter.SearchParams.set(
          searchParams,
          "end",
          isoDate(endDate),
        )
    );

  pathname ++ UniversalRouter.SearchParams.toSearch(searchParams);
};

[@react.component]
let make =
  leaf(() => {
    let main_store = Store.Context.useStore();
    let _unit = main_store.unit;
    let router = UniversalRouter.useRouter();
    let pathname = UniversalRouter.usePathname();
    let searchParams = UniversalRouter.useSearchParams();
    let today = Js.Date.make() |> startOfDay;
    let tomorrow = addDays(today, 1.0);
    let (openDate, closeDate) =
      dateRangeFromSearchParams(~today, ~tomorrow, searchParams);

    let reservationSearchParams =
      UniversalRouter.SearchParams.empty
      |> (
        params =>
          UniversalRouter.SearchParams.set(
            params,
            "start",
            isoDate(openDate),
          )
      )
      |> (
        params =>
          UniversalRouter.SearchParams.set(params, "end", isoDate(closeDate))
      );

    <Container>
      <Card className="bg-slate-200/40 border-slate-200/40 border">
        <h1 className="text-xl">
          <span>
            <Lucide.IconMonitorCloud
              size=48
              className="text-slate-400 mr-2 my-auto inline content-start"
            />
            "Cloud Hardware Rental"->str
          </span>
        </h1>
      </Card>
      <Card
        className="grid grid-cols-[auto_1fr] bg-white/20 gap-4 place-items-start items-center">
        <span className="align-middle text-lg">
          <Lucide.IconClock
            className="text-slate-400 mr-2 my-auto inline content-start"
            size=48
          />
          "Select your reservation type: "->str
        </span>
        <ReservationTypeSelection />
        <div className="col-span-full grid grid-cols-subgrid relative">
          <span className="align-top text-lg">
            <Lucide.IconCalendar
              size=48
              className="mr-2 inline content-start my-auto text-slate-400"
            />
            "Select your reservation time: "->str
          </span>
          <ReactDayPicker
            mode="range"
            selected={
                       `Range(
                         Js.Undefined.return({
                           ReactDayPicker.from: Js.Undefined.return(openDate),
                           ReactDayPicker.to_: Js.Undefined.return(closeDate),
                         }),
                       )
                     }
            onSelect={
                       `Range(
                         (dates: ReactDayPicker.rangeDate) => {
                           switch (dates->Js.Undefined.toOption) {
                           | Some(dates) =>
                             let nextOpenDate =
                               switch (dates.from->Js.Undefined.toOption) {
                               | Some(date) => date
                               | None => today
                               };
                             let nextCloseDate =
                               switch (dates.to_->Js.Undefined.toOption) {
                               | Some(date) => date
                               | None => nextOpenDate
                               };
                             router.push(
                               hrefWithDates(
                                 ~pathname,
                                 ~startDate=nextOpenDate,
                                 ~endDate=nextCloseDate,
                               ),
                             );
                           | None =>
                             router.push(
                               hrefWithDates(
                                 ~pathname,
                                 ~startDate=today,
                                 ~endDate=today,
                               ),
                             )
                           }
                         },
                       )
                     }
          />
        </div>
      </Card>
      <InventoryList openDate closeDate />
      <Cart />
      <div className="w-full">
        <UniversalRouter.RouteLink
          id="cart"
          searchParams=reservationSearchParams
          className="mx-auto mt-4 inline-block bg-slate-500 hover:bg-slate-700 text-white py-2 px-4 rounded-sm text-center">
          "Book Reservation"->str
        </UniversalRouter.RouteLink>
      </div>
    </Container>;
  });
