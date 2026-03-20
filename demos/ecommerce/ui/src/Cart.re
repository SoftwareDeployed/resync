open Tilia.React;

let str = React.string;
let cart_button_class =
  "inline-flex h-8 w-8 items-center justify-center rounded-sm border border-slate-300 bg-white text-slate-700 transition-colors hover:border-slate-400 hover:bg-slate-100";

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
                        ~f=(item: Model.InventoryItem.t) =>
                          item.id == cart_item.inventory_id,
                        main_store.config.inventory,
                      )
                    ) {
                    | Some(item) => item.name
                    | None => cart_item.inventory_id
                    };

                  let%browser_only decrement_item = event => {
                    event->React.Event.toSyntheticEvent->React.Event.Synthetic.preventDefault;
                    CartStore.decrement_item(cart_store, cart_item.inventory_id);
                  };

                  let%browser_only increment_item = event => {
                    event->React.Event.toSyntheticEvent->React.Event.Synthetic.preventDefault;
                    CartStore.increment_item(cart_store, cart_item.inventory_id);
                  };

                  let%browser_only remove_item = event => {
                    event->React.Event.toSyntheticEvent->React.Event.Synthetic.preventDefault;
                    CartStore.remove_item(cart_store, cart_item.inventory_id);
                  };

                  <li
                    key=cart_item.inventory_id
                    className="flex items-center justify-between gap-3 rounded-sm border border-slate-200 bg-white/70 px-3 py-2 text-sm text-slate-700">
                    <div className="flex min-w-0 flex-col">
                      <span className="font-medium"> label->str </span>
                      <span className="text-xs text-slate-500">
                        {("Qty " ++ Int.to_string(cart_item.quantity))->str}
                      </span>
                    </div>
                    <div className="flex items-center gap-2">
                      <button className=cart_button_class onClick=decrement_item>
                        "-"->str
                      </button>
                      <span className="min-w-8 text-center font-medium">
                        {Int.to_string(cart_item.quantity)->str}
                      </span>
                      <button className=cart_button_class onClick=increment_item>
                        "+"->str
                      </button>
                      <button
                        className="ml-2 inline-flex items-center rounded-sm border border-red-200 bg-red-50 px-2 py-1 text-xs font-medium text-red-700 transition-colors hover:border-red-300 hover:bg-red-100"
                        onClick=remove_item>
                        "Remove"->str
                      </button>
                    </div>
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
