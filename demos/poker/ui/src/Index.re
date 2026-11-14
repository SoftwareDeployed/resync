let rootElement = ReactDOM.querySelector("#root");

let%melange_only _ =
  switch (rootElement) {
  | Some(root) =>
    ReactDOM.Client.hydrateRoot(root, <UniversalRouter router=Routes.router state=() />)
    ->ignore
  | None => Js.Console.error("[Poker] No root element found")
  };
