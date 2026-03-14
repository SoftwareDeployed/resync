let str = React.string;

[@react.component]
let make = (~openDate: option(Js.Date.t)=?, ~closeDate: option(Js.Date.t)=?) => {
  let main_store = StoreContext.useStore();
  let config: Config.t = main_store.config;
  let unit: PeriodList.Unit.t = main_store.unit;
  let items = config.inventory;
  let filterType = "all";
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
  let heading =
    switch (unit, openDate == closeDate) {
    | (`Day | `Minute | `Month | `Second | `Week | `Year, true)
    | (`Hour, true) =>
      "Showing "
      ++ filterType
      ++ " equipment available "
      ++ (
        Js.Date.toDateString(openDate) == Js.Date.toDateString(today)
          ? "today" : Js.Date.toLocaleDateString(openDate)
      )
    | (_, false) =>
      "Showing "
      ++ filterType
      ++ " equipment available from "
      ++ Js.Date.toLocaleDateString(openDate)
      ++ " to "
      ++ Js.Date.toLocaleDateString(closeDate)
    };

  let items_by_unit =
    items
    |> Js.Array.filter(~f=(i: Config.InventoryItem.t) =>
         i.period_list
         ->Js.Array.find(~f=pl => pl.unit === PeriodList.Unit.tToJs(unit))
         ->Belt.Option.mapWithDefault(false, _ => true)
       );

  <Card
    className="m-0 p-0 bg-white/30 border-2 border-b-4 border-r-4 border-gray-200/60">
    <h1 className="block align-middle text-lg content-center">
      <ClientOnly>
        {() =>
           <Icon.SearchIcon
             size=48
             className="text-slate-400 mr-2 my-auto inline content-start"
           />}
      </ClientOnly>
      <span className="align-middle"> heading->str </span>
    </h1>
    <Card
      className="border-none shadow-none shadow-transparent m-0 p-0 place-content-start grid lg:grid-cols-4 md:grid-cols-2 sm:grid-cols-1 gap-4">
      {items_by_unit
       ->Js.Array.map(~f=(item: Config.InventoryItem.t) =>
           <InventoryItem key={Int.to_string(item.id)} item />
         )
       ->React.array}
    </Card>
  </Card>;
};
