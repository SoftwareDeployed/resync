let rootElement = ReactDOM.querySelector("#root");

let%browser_only _ =
  switch (rootElement) {
  | Some(domNode) =>
    let store = TodoStore.hydrateStore();

    // Create client-side serverState with empty serializedQueries
    let serverState: Routes.serverState = {store, serializedQueries: ""};

    let result =
      StoreBuilder.Bootstrap.withHydratedProvider(
        ~hydrateStore=() => store,
        ~provider=TodoStore.Context.Provider.make,
        ~children=
          React.array([|
            <UniversalRouter key="router" router=Routes.router state=serverState />,
            <ClientOnly key="toaster"> {() => Sonner.renderToaster()} </ClientOnly>,
          |]),
      );

    ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
  | None => Js.Console.error("[Index.re] No root element found")
  };
