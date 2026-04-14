let rootElement = ReactDOM.querySelector("#root");

let%browser_only _ =
  Js.Console.log("[Index.re] Client bootstrap starting, root element check...");
  switch (rootElement) {
  | Some(domNode) =>
    Js.Console.log("[Index.re] Root element found, calling hydrateStore...");
    let store = TodoStore.hydrateStore();
    Js.Console.log2("[Index.re] Store hydrated, list_id:", store.list_id);

    let result =
      StoreBuilder.Bootstrap.withHydratedProvider(
        ~hydrateStore=() => store,
        ~provider=TodoStore.Context.Provider.make,
        ~children=
          React.array([|
            <UniversalRouter key="router" router=Routes.router state=store />,
            <ClientOnly key="toaster"> {() => Sonner.renderToaster()} </ClientOnly>,
          |]),
      );

    Js.Console.log("[Index.re] Calling hydrateRoot...");
    ReactDOM.Client.hydrateRoot(domNode, result.element)->ignore;
    Js.Console.log("[Index.re] hydrateRoot called");
  | None => Js.Console.error("[Index.re] No root element found")
  };
