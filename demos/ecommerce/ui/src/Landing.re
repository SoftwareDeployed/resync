open Tilia.React;

let str = React.string;

[@react.component]
let make =
  leaf(() => {
    let main_store = StoreContext.useStore();
    let _unit = main_store.unit;
    let today =
      Js.Date.make()
      |> Js.Date.setHours(
           ~hours=0.0,
           ~minutes=0.0,
           ~seconds=0.0,
           ~milliseconds=0.0,
         )
      |> Js.Date.fromFloat;
    let (openDate, setOpenDate) = React.useState(() => today);
    let (closeDate, setCloseDate) = React.useState(() => today);

    let%browser_only calendar = () =>
      <ReactDayPicker
        mode="range"
        selected={
                   `Range(Js.Undefined.return({
                     ReactDayPicker.from: Js.Undefined.return(openDate),
                     ReactDayPicker.to_: Js.Undefined.return(closeDate),
                   }))
                 }
        onSelect={`Range((dates: ReactDayPicker.rangeDate) => {
          switch (dates->Js.Undefined.toOption) {
          | Some(dates) => {
            let openDate = switch (dates.from->Js.Undefined.toOption) {
                           | Some(date) => date
                           | None => today
           };
           let closeDate = switch (dates.to_->Js.Undefined.toOption) {
                           | Some(date) => date
                           | None => openDate
           };
           setOpenDate(_prev => openDate);
           setCloseDate(_prev => closeDate);
           }
           | None => {
             setOpenDate(_prev => today);
             setCloseDate(_prev => today);
           };
          }
        })}
      />;

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
          <ClientOnly> {() => calendar()} </ClientOnly>
        </div>
      </Card>
      <InventoryList openDate closeDate />
      <Cart />
      <div className="w-full">
        <button
          className="mx-auto mt-4 bg-slate-500 hover:bg-slate-700 text-white py-2 px-4 rounded-sm">
          "Book Reservation"->str
        </button>
      </div>
    </Container>;
  });
