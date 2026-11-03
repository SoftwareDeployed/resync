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
    let store = Store.hydrateStore();
    let cartStore = CartStore.hydrateStore();

    let appWithProvider =
      React.createElement(
        Store.Context.Provider.make,
             {
               "value": store,
               "children": React.createElement(
                 CartStore.Context.Provider.make,
                 {
                   "value": cartStore,
                  "children": <UniversalRouter router=Routes.router state=store />,
                 },
               ),
             },
           );

    ReactDOM.Client.hydrateRoot(domNode, appWithProvider)->ignore;
  | None => Js.log("No root element found")
  };
