open Lwt.Syntax;

let getServerState = (context: UniversalRouterDream.serverContext(Store.t)) => {
  let UniversalRouterDream.{ basePath, request } = context;
  let* premise =
    Dream.sql(request, Database.Premise.get_route_premise(basePath));

  switch (premise) {
  | None => Lwt.return(UniversalRouterDream.NotFound)
  | Some(premise) =>
    let premiseId = premise.PeriodList.Premise.id;
    let inventoryPromise =
      if (premiseId == "") {
        Lwt.return([||]);
      } else {
        Dream.sql(request, Database.Inventory.get_list(premiseId));
      };
    let* inventory = inventoryPromise;
    let config: Config.t = {
      inventory,
      premise: Some(premise),
    };
    let store = Store.createStore(config);
    Lwt.return(UniversalRouterDream.State(store));
  };
};

let render = (~context, ~serverState: Store.t, ()) => {
  let store = serverState;
  let serializedState = Store.serializeState(serverState.config);
  let UniversalRouterDream.{
    basePath,
    path: serverPath,
    search: serverSearch,
  } = context;
  let app =
    <UniversalRouter
      router=Routes.router
      state=store
      basePath
      serverPath
      serverSearch
    />;
  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~basePath,
      ~path=serverPath,
      ~search=serverSearch,
      ~serializedState,
      ~state=store,
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
