open Tilia.React;

let str = React.string;

module Count = {
  [@react.component]
  let make =
    leaf(() => {
      useTilia();
      let cart_store = CartStore.Context.useStore();
      Int.to_string(cart_store.item_count)->str;
    });
};

module Contents = {
  [@react.component]
  let make =
    leaf(() => {
      useTilia();
      let cart_store = CartStore.Context.useStore();
      let main_store = Store.Context.useStore();
      let cart_ids = cart_store.items->Js.Dict.keys;

      if (cart_ids->Array.length == 0) {
        <p className="m-2 text-sm text-slate-600">
          "Your cart is empty."->str
        </p>;
      } else {
        <ul className="m-2 flex flex-col gap-2">
          {cart_ids
           |> Array.map(cart_id =>
                switch (cart_store.items->Js.Dict.get(cart_id)) {
                | None => React.null
                | Some(cart_item) =>
                  let label =
                    switch (
                      Js.Array.find(
                        ~f=(item: Config.InventoryItem.t) =>
                          item.id == cart_item.inventory_id,
                        main_store.config.inventory,
                      )
                    ) {
                    | Some(item) => item.name
                    | None => cart_item.inventory_id
                    };

                  <li
                    key=cart_item.inventory_id
                    className="flex items-center justify-between rounded-sm border border-slate-200 bg-white/70 px-3 py-2 text-sm text-slate-700">
                    <span> label->str </span>
                    <span className="font-medium">
                      {("Qty " ++ Int.to_string(cart_item.quantity))->str}
                    </span>
                  </li>
                }
              )
           |> React.array}
        </ul>;
      };
    });
};

[@react.component]
let make = (~showContents=false) => {
  <div>
    <h1 className="block font-bold align-middle text-gray-700 m-2 text-3xl">
      <UniversalRouter.NavLink
        id="cart"
        exact=true
        className="inline-flex items-center text-gray-700 transition-colors hover:text-slate-900"
        activeClassName="text-slate-900">
        <span className="m-2 align-middle text-3xl font-light">
          <Lucide.IconShoppingCart size=24 />
        </span>
        {str("Selected equipment (")}
        <ClientOnly> {() => <Count />} </ClientOnly>
        {str(")")}
      </UniversalRouter.NavLink>
    </h1>
    {showContents ? <ClientOnly> {() => <Contents />} </ClientOnly> : React.null}
  </div>;
};
