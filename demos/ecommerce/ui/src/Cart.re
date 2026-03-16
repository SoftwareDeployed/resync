open Tilia.React;

let str = React.string;

[@react.component]
let make =
  leaf(() => {
  useTilia();
  let cart_store = CartStore.Context.useStore();

  <h1 className="block font-bold align-middle text-gray-700 m-2 text-3xl">
    <span className="m-2 align-middle text-3xl font-light">
      <Lucide.Icon name="shopping-cart" size=24 />
    </span>
    {str("Selected equipment (")}
    <ClientOnly>
      {() => {
        let count = cart_store.item_count;
        str(Int.to_string(count));
      }}
    </ClientOnly>
    {str(")")}
  </h1>;
});
