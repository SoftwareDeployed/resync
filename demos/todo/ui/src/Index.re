[@platform js]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@platform native]
module Style = {
  let _css = ();
};

let root =
  switch (ReactDOM.querySelector("#root")) {
  | Some(el) => el
  | None => failwith("No root element found")
  };

let store = TodoStore.hydrateStoreWithLogs();

let app =
  React.createElement(
    TodoStore.Context.Provider.make,
    {
      "value": store,
      "children": <UniversalRouter router=Routes.router state=store />,
    },
  );

ReactDOM.Client.hydrateRoot(root, app) |. ignore;
