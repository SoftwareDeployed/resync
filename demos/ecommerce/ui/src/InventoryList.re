open Tilia.React;

let str = React.string;

[@react.component]
let make =
  leaf((~openDate: option(Js.Date.t)=?, ~closeDate: option(Js.Date.t)=?) => {
  let main_store = Store.Context.useStore();
  let config: Model.t = main_store.config;
  let unit: PeriodList.Unit.t = main_store.unit;
  let items = config.inventory;
  let (query, setQuery) = React.useState(() => "");
  let today = Dates.today();
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
    switch (is_same_day_selection) {
    | true =>
      "Showing "
      ++ "all"
      ++ " equipment available "
      ++ (
        String.equal(Js.Date.toDateString(openDate), Js.Date.toDateString(today))
          ? "today" : DateFormatter.formatDate(openDate)
      )
    | false =>
      "Showing "
      ++ "all"
      ++ " equipment available from "
      ++ DateFormatter.formatDate(openDate)
      ++ " to "
      ++ DateFormatter.formatDate(closeDate)
    };

  let selected_unit = PeriodList.Unit.tToJs(unit);

  let%browser_only on_search_change = event => {
    let value = React.Event.Form.target(event)##value;
    setQuery(_previous => value);
  };

  let normalized_query =
    query->Js.String.toLowerCase->Js.String.trim;
  let matches_query = (item: Model.InventoryItem.t) =>
    normalized_query == ""
    || {
      let name = item.name->Js.String.toLowerCase;
      let description = item.description->Js.String.toLowerCase;
      name->Js.String.includes(~search=normalized_query)
      || description->Js.String.includes(~search=normalized_query);
    };

  let items_by_unit =
    items
    ->Js.Array.filter(~f=(item: Model.InventoryItem.t) =>
        item.period_list->Js.Array.some(~f=(period: Model.Pricing.period) =>
          period.unit == selected_unit
        )
      )
    ->Js.Array.filter(~f=matches_query);

  let result_count = items_by_unit->Array.length;
  let result_label =
    normalized_query == ""
      ? ""
      : " ("
        ++ Int.to_string(result_count)
        ++ " matching \""
        ++ query
        ++ "\")";

  <Card
    className="m-0 p-0 bg-white/30 border-2 border-b-4 border-r-4 border-gray-200/60">
    <h1 className="block align-middle text-lg content-center">
      <Lucide.IconSearch
        size=48
        className="text-slate-400 mr-2 my-auto inline content-start"
      />
      <span className="align-middle"> {(heading ++ result_label)->str} </span>
    </h1>
    <div className="m-2 flex items-center gap-2">
      <input
        id="inventory-search"
        type_="text"
        placeholder="Search equipment by name or description..."
        value=query
        onChange=on_search_change
        className="w-full max-w-md rounded-sm border border-slate-300 bg-white px-3 py-2 text-sm text-slate-700 placeholder:text-slate-400 focus:border-slate-500 focus:outline-none"
      />
      {
        normalized_query != ""
          ? <button
              id="inventory-search-clear"
              className="rounded-sm border border-slate-300 bg-white px-2 py-2 text-xs text-slate-600 hover:bg-slate-100"
              onClick=(_event => setQuery(_previous => ""))>
              "Clear"->str
            </button>
          : React.null
      }
    </div>
    <Card
      className="border-none shadow-none shadow-transparent m-0 p-0 place-content-start grid lg:grid-cols-4 md:grid-cols-2 sm:grid-cols-1 gap-4">
      {items_by_unit
       ->Js.Array.map(~f=(item: Model.InventoryItem.t) =>
              <InventoryItem key={item.id} item />
            )
       |> React.array}
    </Card>
  </Card>;
});
