open Tilia.React;

let str = React.string;

[@react.component]
let make =
  leaf(() => {
  useTilia();
  let cart_store = CartStore.Context.useStore();

  <h1 className="block font-bold align-middle text-gray-700 m-2 text-3xl">
    <span className="m-2 align-middle text-3xl font-light">
      <ClientOnly> {() => <Icon.CartIcon size=24 />} </ClientOnly>
    </span>
    <ClientOnly>
      {() => {
        let count = cart_store.item_count;
        <>
          {str("Selected equipment (")}
          {str(Int.to_string(count))}
          {str(")")}
        </>;
      }}
    </ClientOnly>
  </h1>;
});
