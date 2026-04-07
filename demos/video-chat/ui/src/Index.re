[@platform js]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@platform native]
module Style = {
  let _css = ();
};

let rootElement = ReactDOM.querySelector("#root");

let%browser_only _ =
  switch (rootElement) {
  | Some(domNode) =>
    let store = VideoChatStore.hydrateStore();

    let appWithProvider =
      React.createElement(
        VideoChatStore.Context.Provider.make,
        {
          "value": store,
          "children":
            React.array([|
              <UniversalRouter key="router" router=Routes.router state=store />,
              <ClientOnly key="toaster"> {() => Sonner.renderToaster()} </ClientOnly>,
            |]),
        },
      );

    ReactDOM.Client.createRoot(domNode)->ReactDOM.Client.render(appWithProvider);
  | None => ()
  };
