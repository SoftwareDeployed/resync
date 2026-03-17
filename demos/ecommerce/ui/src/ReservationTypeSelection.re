let str = React.string;

[@react.component]
let make =
  Tilia.React.leaf(() => {
    let main_store = StoreContext.useStore();
    let%browser_only select_unit = (period: Config.Pricing.period, e) => {
      let inputEl = e->React.Event.Form.currentTarget;
      if (inputEl##checked == true) {
        switch (PeriodList.Unit.tFromJs(period.unit)) {
        | Some(unit) => PeriodList.Unit.set(unit)
        | None => ()
        };
      };
    };
    <div className="my-auto">
      {main_store.period_list
       |> Array.map((period: Config.Pricing.period) =>
            switch (PeriodList.Unit.tFromJs(period.unit)) {
            | None => React.null
            | Some(unit) =>
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
                  checked={main_store.unit == unit}
                  autoComplete="off"
                />
                <span className="p-1 pl-0"> period.label->str </span>
              </label>
            }
          )
       |> React.array}
    </div>;
  });
