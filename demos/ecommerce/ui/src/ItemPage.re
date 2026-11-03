open Tilia.React;

let str = React.string;

[@react.component]
let make =
  leaf(() => {
    let main_store = Store.Context.useStore();
    let router = UniversalRouter.useRouter();
    let searchParams = UniversalRouter.useSearchParams();
    let params = UniversalRouter.useParams();
    let cart_store = CartStore.Context.useStore();
    let id =
      switch (UniversalRouter.Params.find("id", params)) {
      | Some(id) => id
      | None => ""
      };

    let reservation = CartStore.reservationFromSearchParams(searchParams);

    switch (Store.findItem(main_store.config, id)) {
    | None =>
      <Card className="bg-white/40">
        <h1 className="m-2 text-xl font-bold text-slate-700">
          "Item not found"->str
        </h1>
        <p className="m-2 text-sm text-slate-600">
          "This equipment is no longer available."->str
        </p>
        <UniversalRouter.RouteLink
          id="home"
          searchParams
          className="m-2 inline-block text-sm text-slate-700 underline underline-offset-4 hover:text-slate-900">
          "Back to all equipment"->str
        </UniversalRouter.RouteLink>
      </Card>;
    | Some(item) => {
      let image = "https://random.danielpetrica.com/api/random?" ++ item.id;
      let unit = main_store.unit;
      let selected_unit = PeriodList.Unit.tToJs(unit);
      let periods =
        item.period_list->Js.Array.filter(~f=(period: Model.Pricing.period) =>
          period.unit == selected_unit
        );
      let%melange_only add_to_cart_click = event => {
        event->React.Event.toSyntheticEvent->React.Event.Synthetic.preventDefault;
        CartStore.add_to_cart(
          cart_store,
          item,
          ~maxQuantity=item.quantity,
          ~reservation,
          (),
        );
        router.push(
          "/cart" ++ UniversalRouter.SearchParams.toSearch(searchParams),
        );
      };
      <Container>
        <Card className="bg-white/40">
          <div className="grid gap-4 md:grid-cols-2">
            <div className="flex flex-col">
              <img
                id={"item-detail-image-" ++ item.id}
                className="w-full rounded-sm border border-slate-200 aspect-square"
                src=image
              />
              <h1
                id="item-detail-name"
                className="mt-2 text-2xl font-bold text-slate-800">
                item.name->str
              </h1>
              <p className="mt-2 text-sm text-slate-600">
                item.description->str
              </p>
            </div>
            <div className="flex flex-col gap-3">
              <div
                className="flex items-center justify-between rounded-sm border border-slate-200 bg-white/70 px-3 py-2">
                <span className="text-sm font-medium text-slate-700">
                  "Availability"->str
                </span>
                <span
                  id={"inventory-stock-" ++ item.id}
                  className="text-sm text-slate-600">
                  {("Stock: " ++ Int.to_string(item.quantity))->str}
                </span>
              </div>
              <Pricing period_list={periods} />
              {switch (reservation) {
               | Some(reservation) =>
                 <div
                   id="item-detail-reservation"
                   className="rounded-sm border border-slate-200 bg-white/70 px-3 py-2 text-sm text-slate-700">
                   {("Reserving for "
                     ++ reservation.start_date
                     ++ " to "
                     ++ reservation.end_date)->str}
                 </div>
               | None => React.null
               }}
              <button
                id="item-detail-add-to-cart"
                className="rounded-sm bg-slate-500 px-4 py-3 text-sm font-medium text-white hover:bg-slate-700"
                onClick=add_to_cart_click>
                "Add to cart and review reservation"->str
              </button>
              <UniversalRouter.RouteLink
                id="home"
                searchParams
                className="inline-block text-sm text-slate-700 underline underline-offset-4 hover:text-slate-900">
                "Back to all equipment"->str
              </UniversalRouter.RouteLink>
            </div>
          </div>
        </Card>
      </Container>;
      }
    }
  });
