open Tilia.React;

let str = React.string;

[@react.component]
let make =
  leaf(() => {
    let main_store = Store.Context.useStore();
    let _unit = main_store.unit;
    let router = UniversalRouter.useRouter();
    let pathname = UniversalRouter.usePathname();
    let searchParams = UniversalRouter.useSearchParams();
    let today = Dates.today();
    let tomorrow = Dates.addDays(today, 1.0);
    let (openDate, closeDate) =
      Dates.rangeFromSearchParams(~today, ~tomorrow, searchParams);

    let reservationSearchParams =
      Dates.searchParamsForRange(~startDate=openDate, ~endDate=closeDate);

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
                               Dates.hrefWithDates(
                                 ~pathname,
                                 ~startDate=nextOpenDate,
                                 ~endDate=nextCloseDate,
                               ),
                             );
                           | None =>
                             router.push(
                               Dates.hrefWithDates(
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
