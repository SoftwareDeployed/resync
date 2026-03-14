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
    // Hydrate store from DOM JSON and wrap in Tilia
    let store = StoreHydration.hydrateStore();
    
    // Wrap App in StoreContext.Provider with hydrated store
    let appWithProvider =
      React.createElement(
        StoreContext.Provider.make,
        {"value": store, "children": <App />}
      );
    
    ReactDOM.Client.hydrateRoot(domNode, appWithProvider)->ignore
  | None => Js.log("No root element found")
  };
