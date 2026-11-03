[@melange_only]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@native_only]
module Style = {
  let _css = ();
};

let root =
  switch (ReactDOM.querySelector("#root")) {
  | Some(el) => el
  | None => failwith("No root element found")
  };

let store = TodoStore.hydrateStore();

let app =
  React.createElement(
    TodoStore.Context.Provider.make,
    {
      "value": store,
      "children": <UniversalRouter router=Routes.router state=store />,
    },
  );

ReactDOM.Client.hydrateRoot(root, app) |. ignore;
