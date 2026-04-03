let rootElement = ReactDOM.querySelector("#root");

let%browser_only _ =
  switch (rootElement) {
  | Some(domNode) =>
    let store = TodoStore.hydrateStore();

    let appWithProvider =
      React.createElement(
        TodoStore.Context.Provider.make,
        {
          "value": store,
          "children":
            <UniversalRouter router=Routes.router state=store />,
        },
      );

    ReactDOM.Client.hydrateRoot(domNode, appWithProvider)->ignore;
  | None => Js.log("No root element found")
  };
