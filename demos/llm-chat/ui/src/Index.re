let rootElement = ReactDOM.querySelector("#root");

let%browser_only _ =
  switch (rootElement) {
  | Some(domNode) =>
    let store = LlmChatStore.hydrateStore();

    let result =
      StoreBuilder.Bootstrap.withHydratedProvider(
        ~hydrateStore=() => store,
        ~provider=LlmChatStore.Context.Provider.make,
        ~children=
          React.array([|
            <UniversalRouter key="router" router=Routes.router state=store />,
          |]),
      );

    ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
  | None => Js.log("No root element found")
  };
