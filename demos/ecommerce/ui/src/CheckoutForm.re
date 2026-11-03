open Tilia.React;

let str = React.string;

/* The React DOM binding does not expose form method/action attributes on
   both targets; configure the native form behavior imperatively instead. */
[@platform js]
[@mel.send]
external setAttributeOn: (Dom.element, string, string) => unit = "setAttribute";

[@platform js]
[@mel.send]
external requestSubmitElement: Dom.element => unit = "requestSubmit";

[@platform js]
let configureCheckoutForm = () =>
  switch (Webapi.Dom.document |> Webapi.Dom.Document.getElementById("checkout-form")) {
  | Some(element) =>
    setAttributeOn(element, "method", "POST");
    setAttributeOn(element, "action", "/api/checkout");
  | None => ()
  };

[@platform native]
let configureCheckoutForm = () => ();



/* Client-only checkout form. Cart lines live in IndexedDB, so the item
   inputs are rendered on the client; submitting POSTs a plain HTML form to
   /api/checkout, which creates the order server-side and redirects to
   /order/:id. The cart is cleared optimistically on submit. */
module Form = {
  [@react.component]
  let make =
    leaf((~searchParams: UniversalRouter.SearchParams.t) => {
      useTilia();
      let cart_store = CartStore.Context.useStore();
      let main_store = Store.Context.useStore();
      let cart_ids = cart_store.state.items->Js.Dict.keys;
      let start =
        switch (UniversalRouter.SearchParams.get("start", searchParams)) {
        | Some(value) => value
        | None => ""
        };
      let end_date =
        switch (UniversalRouter.SearchParams.get("end", searchParams)) {
        | Some(value) => value
        | None => ""
        };
      let premise_id = main_store.premise_id;
      let selected_unit = PeriodList.Unit.tToJs(main_store.unit);

      React.useEffect0(() => {
        configureCheckoutForm();
        None;
      });

      let%browser_only on_confirm_click = _event => {
        /* React 19 intercepts native submit events, so the confirm button is
           a plain button that submits the form programmatically. */
        switch (Webapi.Dom.document |> Webapi.Dom.Document.getElementById("checkout-form")) {
        | Some(form) => requestSubmitElement(form)
        | None => ()
        };
      };

      if (cart_ids->Array.length == 0) {
        <p id="checkout-empty" className="m-2 text-sm text-slate-600">
          "Add equipment to your cart to book a reservation."->str
        </p>;
      } else {
        <form
          id="checkout-form"
          className="m-2 flex flex-col gap-2 rounded-sm border border-slate-300 bg-white/70 p-3">
          <h2 className="text-base font-bold text-slate-800">
            "Book your reservation"->str
          </h2>
          <input type_="hidden" name="premise_id" value=premise_id />
          <input type_="hidden" name="start" value=start />
          <input type_="hidden" name="end" value=end_date />
          <input type_="hidden" name="unit" value=selected_unit />
          {cart_ids
           ->Js.Array.map(~f=inventory_id =>
                switch (cart_store.state.items->Js.Dict.get(inventory_id)) {
                | None => React.null
                | Some((cart_item: CartStore.CartItem.t)) =>
                  <input
                    key=inventory_id
                    type_="hidden"
                    name={"item_" ++ inventory_id}
                    value={Int.to_string(cart_item.quantity)}
                  />
                }
              )
           |> React.array}
          <div className="grid gap-2 sm:grid-cols-2">
            <label className="flex flex-col gap-1 text-sm text-slate-700">
              "Name"->str
              <input
                id="checkout-customer-name"
                name="customer_name"
                required=true
                placeholder="Ada Lovelace"
                className="rounded-sm border border-slate-300 px-3 py-2 text-sm focus:border-slate-500 focus:outline-none"
              />
            </label>
            <label className="flex flex-col gap-1 text-sm text-slate-700">
              "Email"->str
              <input
                id="checkout-customer-email"
                name="customer_email"
                type_="email"
                required=true
                placeholder="ada@example.com"
                className="rounded-sm border border-slate-300 px-3 py-2 text-sm focus:border-slate-500 focus:outline-none"
              />
            </label>
          </div>
          <div className="flex items-center gap-3">
            <button
              id="checkout-submit"
              type_="button"
              className="rounded-sm bg-slate-500 px-4 py-2 text-sm font-medium text-white hover:bg-slate-700"
              onClick=on_confirm_click>
              "Confirm reservation"->str
            </button>
            <button
              id="cart-clear"
              type_="button"
              className="rounded-sm border border-red-200 bg-red-50 px-3 py-2 text-xs font-medium text-red-700 hover:border-red-300 hover:bg-red-100"
              onClick=(_event => CartStore.clear())>
              "Clear cart"->str
            </button>
          </div>
        </form>;
      };
    });
};

[@react.component]
let make = (~searchParams) => {
  let start =
    switch (UniversalRouter.SearchParams.get("start", searchParams)) {
    | Some(value) => Some(value)
    | None => None
    };
  let end_date =
    switch (UniversalRouter.SearchParams.get("end", searchParams)) {
    | Some(value) => Some(value)
    | None => None
    };
  let reservation_label =
    switch (start, end_date) {
    | (Some(start), Some(end_date)) => " for " ++ start ++ " to " ++ end_date
    | _ => ""
    };
  <div>
    <p className="m-2 text-sm text-slate-600">
      {("Enter your details to confirm the reservation" ++ reservation_label ++ ".")->str}
    </p>
    <ClientOnly>
      {() => <Form searchParams />}
    </ClientOnly>
  </div>;
};
