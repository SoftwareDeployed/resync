open Tilia.React;

let str = React.string;

[@react.component]
let make =
    leaf((~openDate: option(Js.Date.t)=?, ~closeDate: option(Js.Date.t)=?) => {
  let main_store = StoreContext.useStore();
  let config: Config.t = main_store.config;
  let unit: PeriodList.Unit.t = main_store.unit;
  let items = config.inventory;
  let today =
    Js.Date.make()
    |> Js.Date.setHours(
         ~hours=0.0,
         ~minutes=0.0,
         ~seconds=0.0,
         ~milliseconds=0.0,
       )
    |> Js.Date.fromFloat;
  let openDate =
    switch (openDate) {
    | Some(date) => date
    | _ => today
    };
  let closeDate =
    switch (closeDate) {
    | Some(date) => date
    | _ => today
    };
  let is_same_day_selection =
    Float.equal(Js.Date.getTime(openDate), Js.Date.getTime(closeDate));
  let heading =
    switch (unit, is_same_day_selection) {
    | (_, true) =>
      "Showing "
      ++ "all"
      ++ " equipment available "
      ++ (
        String.equal(Js.Date.toDateString(openDate), Js.Date.toDateString(today))
          ? "today" : Js.Date.toLocaleDateString(openDate)
      )
    | (_, false) =>
      "Showing "
      ++ "all"
      ++ " equipment available from "
      ++ Js.Date.toLocaleDateString(openDate)
      ++ " to "
      ++ Js.Date.toLocaleDateString(closeDate)
    };

  let selected_unit = PeriodList.Unit.tToJs(unit);

  let items_by_unit =
    items
    |> Array.fold_left((matching, item: Config.InventoryItem.t) =>
         if (
           Array.exists(
             (period: Config.Pricing.period) => period.unit == selected_unit,
             item.period_list,
           )
         ) {
           [item, ...matching];
         } else {
           matching;
         }, [])
    |> List.rev
    |> Array.of_list;

  <Card
    className="m-0 p-0 bg-white/30 border-2 border-b-4 border-r-4 border-gray-200/60">
    <h1 className="block align-middle text-lg content-center">
      <Lucide.Icon
        name="search"
        size=48
        className="text-slate-400 mr-2 my-auto inline content-start"
      />
      <span className="align-middle"> heading->str </span>
    </h1>
    <Card
      className="border-none shadow-none shadow-transparent m-0 p-0 place-content-start grid lg:grid-cols-4 md:grid-cols-2 sm:grid-cols-1 gap-4">
      {items_by_unit
       |> Array.map((item: Config.InventoryItem.t) =>
             <InventoryItem key={item.id} item />
           )
       |> React.array}
    </Card>
  </Card>;
});
