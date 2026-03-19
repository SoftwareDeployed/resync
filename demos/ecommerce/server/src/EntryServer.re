let handler = (~routeRoot, ~serverPath, ~serverSearch="", config: Config.t) => {
  let store = Store.createStore(config);
  let serializedState = Store.serializeState(config);
  let app =
    <UniversalRouter
      router=Routes.router
      routeRoot
      serverPath
      serverSearch
    />;
  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~routeRoot,
      ~path=serverPath,
      ~search=serverSearch,
      ~serializedState,
      (),
    );

  <Store.Context.Provider value=store>
    <CartStore.Context.Provider value=CartStore.empty>
      document
    </CartStore.Context.Provider>
  </Store.Context.Provider>;
};
