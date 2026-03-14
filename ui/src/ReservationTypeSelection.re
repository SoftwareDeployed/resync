let str = React.string;

[@react.component]
let make = () => {
  let main_store = Store.useStore();
  let%browser_only select_unit = (period: Config.Pricing.period, e) => {
    let inputEl = e->React.Event.Form.currentTarget;
    if (inputEl##checked == true) {
      PeriodList.Unit.set(PeriodList.Unit.tFromJs(period.unit)->Option.get);
    };
  };
  <div className="my-auto">
    {main_store.period_list
     |> Array.map((period: Config.Pricing.period) =>
          <label
            className="block"
            key={period.unit}
            htmlFor={"type_" ++ period.unit}>
            <input
              className="m-1"
              id={"type_" ++ period.unit}
              name="type"
              type_="radio"
              value="hour"
              onChange={select_unit(period)}
              checked={
                main_store.unit
                == PeriodList.Unit.tFromJs(period.unit)->Option.get
              }
              autoComplete="off"
            />
            <span className="p-1 pl-0"> period.label->str </span>
          </label>
        )
     |> React.array}
  </div>;
};
