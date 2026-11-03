open Tilia.React;

let str = React.string;

let format_price = (cents: int) =>
  (Int.to_float(cents) /. 100.0)->NumberFormatter.formatCurrency;

[@react.component]
let make =
  leaf(() => {
    let main_store = Store.Context.useStore();

    /* The order is confirmed: empty the local cart that produced it.
       (Hooks must run unconditionally, before the switch below.) */
    React.useEffect0(() => {
      switch (main_store.config.order) {
      | Some(_) => CartStore.clear()
      | None => ()
      };
      None;
    });

    switch (main_store.config.order) {
    | None =>
      <Card className="bg-white/40">
        <h1 className="m-2 text-xl font-bold text-slate-700">
          "Order not found"->str
        </h1>
        <UniversalRouter.RouteLink
          id="home"
          className="m-2 inline-block text-sm text-slate-700 underline underline-offset-4 hover:text-slate-900">
          "Back to all equipment"->str
        </UniversalRouter.RouteLink>
      </Card>;
    | Some(order) => {
      let item_count = order.items->Array.length;
      let total_cents =
        order.items
        ->Js.Array.reduce(
            ~init=0,
            ~f=(total, (item: Model.OrderItem.t)) =>
              total + item.quantity * item.unit_price_cents,
          );
      <Card className="bg-white/40">
        <h1
          id="order-confirmation-title"
          className="m-2 text-xl font-bold text-slate-800">
          "Reservation confirmed"->str
        </h1>
        <p className="m-2 text-sm text-slate-600">
          {("Order "
            ++ String.sub(order.id, 0, 8)
            ++ " is "
            ++ order.status
            ++ ". A confirmation was sent to "
            ++ order.customer_email
            ++ ".")->str}
        </p>
        <div
          className="m-2 rounded-sm border border-slate-200 bg-white/70 px-3 py-2 text-sm text-slate-700">
          <div className="flex justify-between">
            <span className="font-medium"> "Rental window"->str </span>
            <span id="order-dates">
              {(order.start_date ++ " to " ++ order.end_date)->str}
            </span>
          </div>
          <div className="mt-1 flex justify-between">
            <span className="font-medium"> "Customer"->str </span>
            <span id="order-customer"> order.customer_name->str </span>
          </div>
        </div>
        <ul className="m-2 flex flex-col gap-2">
          {order.items
           ->Js.Array.map(~f=(item: Model.OrderItem.t) =>
                  <li
                    key=item.inventory_id
                    className="flex items-center justify-between gap-3 rounded-sm border border-slate-200 bg-white/70 px-3 py-2 text-sm text-slate-700">
                    <div className="flex min-w-0 flex-col">
                      <span className="font-medium"> item.name->str </span>
                      <span className="text-xs text-slate-500">
                        {("Qty "
                          ++ Int.to_string(item.quantity)
                          ++ " × "
                          ++ item.unit_label)->str}
                      </span>
                    </div>
                    <span className="font-medium">
                      {format_price(item.quantity * item.unit_price_cents)->str}
                    </span>
                  </li>
                )
           |> React.array}
        </ul>
        <div
          id="order-total"
          className="m-2 flex items-center justify-between rounded-sm border border-slate-300 bg-white px-3 py-2 text-base font-bold text-slate-800">
          <span> "Total"->str </span>
          <span> {format_price(total_cents)->str} </span>
        </div>
        <p className="m-2 text-xs text-slate-500">
          {Int.to_string(item_count)->str}
          {" item(s) reserved. Stock levels update live for all visitors."->str}
        </p>
        <UniversalRouter.RouteLink
          id="home"
          className="m-2 inline-block text-sm text-slate-700 underline underline-offset-4 hover:text-slate-900">
          "Back to all equipment"->str
        </UniversalRouter.RouteLink>
      </Card>;
      }
    }
  });
