[@react.component]
let make = (~period_list: Config.Pricing.period_list) => {
  let format_price = (p: int) => {
    (Int.to_float(p) /. Int.to_float(100))
    ->NumberFormatter.formatCurrency
    ->React.string;
  };

  let key_for_period = (p: Config.Pricing.period) =>
    p.unit ++ ":" ++ p.label ++ ":" ++ Int.to_string(p.id);

  let list_items =
    period_list
    |> Array.map((p: Config.Pricing.period) => {
         <React.Fragment key={key_for_period(p)}>
           <div className="border-1 border-black/30 p-1">
             <p className="text-left text-sm"> {React.string(p.label)} </p>
           </div>
           <div className="border-1 border-black/30 p-1">
             <p className="text-left text-sm"> {format_price(p.price)} </p>
           </div>
         </React.Fragment>
       });

  <div
    className="w-full rounded-lg border border-slate-200/50 grid grid-cols-2 bg-white">
    <div className="border border-slate-200/50 bg-black/30 text-white p-1">
      <p className="font-bold text-left"> "Unit"->React.string </p>
    </div>
    <div className="border border-slate-200/50 bg-black/30 text-white p-1">
      <p className="font-bold text-left"> "Price"->React.string </p>
    </div>
    {list_items |> React.array}
  </div>;
};
