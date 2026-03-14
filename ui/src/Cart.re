let str = React.string;

[@react.component]
let make = () => {
  let main_store = Store.useStore();
  let config: Config.t = main_store.config;
  let cart = Store.CartStore.state;
  let count = cart.items->Js.Dict.keys->Array.length;
  let _items = config.inventory;

  <h1 className="block font-bold align-middle text-gray-700 m-2 text-3xl">
    <span className="m-2 align-middle text-3xl font-light">
      <ClientOnly> {() => <Icon.CartIcon size=24 />} </ClientOnly>
    </span>
    {str("Selected equipment (")}
    {str(Int.to_string(count))}
    {str(")")}
  </h1>;
};
