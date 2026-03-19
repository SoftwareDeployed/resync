open Lwt.Syntax;

let getServerState = (context: UniversalRouterDream.serverContext) => {
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let* premise =
    Dream.sql(
      UniversalRouterDream.contextRequest(context),
      Database.Premise.get_route_premise(routeRoot),
    );

  switch (premise) {
  | None => Lwt.return(UniversalRouterDream.NotFound)
  | Some(premise) =>
    let premiseId = premise.PeriodList.Premise.id;
    let inventoryPromise =
      if (premiseId == "") {
        Lwt.return([||]);
      } else {
        Dream.sql(
          UniversalRouterDream.contextRequest(context),
          Database.Inventory.get_list(premiseId),
        );
      };
    let* inventory = inventoryPromise;
    let config: Config.t = {inventory, premise: Some(premise)};
    Lwt.return(UniversalRouterDream.State(config))
  };
};

let render = (~context, ~serverState: Config.t, ()) => {
  let store = Store.createStore(serverState);
  let serializedState = Store.serializeState(serverState);
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let serverPath = UniversalRouterDream.contextPath(context);
  let serverSearch = UniversalRouterDream.contextSearch(context);
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

let app =
  UniversalRouterDream.app(
    ~router=Routes.router,
    ~getServerState,
    ~render,
    (),
  );
