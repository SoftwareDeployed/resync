open Tilia.React;

let str = React.string;

[@react.component]
let make =
  leaf((~item: option(Model.InventoryItem.t)=?) => {
    let item =
      switch (item) {
      | Some(item) => item
      | None =>
          Js.Exn.raiseError("The item property is required on InventoryItem")
      };
    let cart_store = CartStore.Context.useStore();
    let item_params = UniversalRouter.Params.ofList([("id", item.id)]);
    let searchParams = UniversalRouter.useSearchParams();
    let image =
      "https://random.danielpetrica.com/api/random?" ++ item.id;
    let%browser_only add_to_cart_click = e => {
      e->React.Event.toSyntheticEvent->React.Event.Synthetic.preventDefault;
      Js.log2("[cart] add button click", item.id);
      CartStore.add_to_cart(cart_store, item, ~maxQuantity=item.quantity, ());
    };
    let add_to_cart_button =
      switch%platform (Runtime.platform) {
      | Client =>
        <button
          className="mx-[1.5] mb-[1.5] rounded-sm bg-slate-500 px-3 py-2 text-sm text-white hover:bg-slate-700"
          onClick=add_to_cart_click>
          "Add to cart"->str
        </button>
      | Server =>
        <button
          className="mx-[1.5] mb-[1.5] rounded-sm bg-slate-500 px-3 py-2 text-sm text-white hover:bg-slate-700">
          "Add to cart"->str
        </button>
      };
    <div className="flex flex-1 flex-col grow border-2">
      <UniversalRouter.RouteLink
        id="item"
        replace=true
        params=item_params
        searchParams
        elementId={"item-" ++ item.id}
        className="relative m-[1.5] flex flex-1 flex-col grow">
        <div className="rounded-sm shadow-sm m-0 p-0">
          <img className="p-[1.5] w-full aspect-square" src=image />
        </div>
        <div
          className="flex flex-row justify-between w-full bg-gray-300 text-white shadow-sm">
          <h2 className="tracking-wider text-xs px-2"> item.name->str </h2>
        </div>
         <div
           className="flex flex-col grow flex-1 w-full bg-white/40 rounded-sm m-[1.5] justify-between items-end">
           <p className="text-xs text-left m-2"> item.description->str </p>
           <span
             id={"inventory-stock-" ++ item.id}
             className="text-xs text-slate-600 m-2">
             {("Stock: " ++ Int.to_string(item.quantity))->str}
           </span>
           <Pricing period_list={item.period_list} />
         </div>
      </UniversalRouter.RouteLink>
      add_to_cart_button
    </div>;
  });
