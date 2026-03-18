let handler = (~routeRoot, ~serverPath, ~serverSearch="", config: Config.t) => {
  let store = Store.createStore(config);
  let serializedState = Store.serializeState(config);
  let app = <App routeRoot serverPath serverSearch />;
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

  <StoreContext.Provider value=store>
    <CartStore.Context.Provider value=CartStore.empty>
      document
    </CartStore.Context.Provider>
  </StoreContext.Provider>;
};
