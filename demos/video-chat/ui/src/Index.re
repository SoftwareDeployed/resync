[@melange_only]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@native_only]
module Style = {
  let _css = ();
};

let rootElement = ReactDOM.querySelector("#root");

let%melange_only _ =
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
