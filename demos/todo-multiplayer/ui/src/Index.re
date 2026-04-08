let rootElement = ReactDOM.querySelector("#root");

let%browser_only _ =
  switch (rootElement) {
  | Some(domNode) =>
    let store = TodoStore.hydrateStore();

    let result =
      StoreBuilder.Bootstrap.withHydratedProvider(
        ~hydrateStore=() => store,
        ~provider=TodoStore.Context.Provider.make,
        ~children=
          React.array([|
            <UniversalRouter key="router" router=Routes.router state=store />,
            <ClientOnly key="toaster"> {() => Sonner.renderToaster()} </ClientOnly>,
          |]),
      );

    ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
  | None => Js.log("No root element found")
  };
