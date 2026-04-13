open Lwt.Syntax;

let getServerState = (context: UniversalRouterDream.serverContext(Store.t)) => {
  let UniversalRouterDream.{ basePath, request } = context;
  let* premiseRow =
    Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
      RealtimeSchema.Queries.GetRoutePremise.find_opt(
        (module Db),
        RealtimeSchema.Queries.GetRoutePremise.caqti_type,
        basePath,
      )
    );

  switch (premiseRow) {
  | None => Lwt.return(UniversalRouterDream.NotFound)
  | Some(row) =>
    let premise = ({
      PeriodList.Premise.id: row.id,
      name: row.name,
      description: row.description,
      updated_at: Js.Date.fromFloat(row.updated_at),
    }: PeriodList.Premise.t);
    let premiseId = premise.PeriodList.Premise.id;
    let inventoryPromise =
      if (premiseId == "") {
        Lwt.return([]);
      } else {
        Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
          RealtimeSchema.Queries.GetInventoryList.collect(
            (module Db),
            RealtimeSchema.Queries.GetInventoryList.caqti_type,
            premiseId,
          )
        );
      };
    let* inventoryRows = inventoryPromise;
    let inventory =
      Array.map(
        (itemRow: RealtimeSchema.Queries.GetInventoryList.row) => ({
          Model.InventoryItem.description: itemRow.description,
          id: itemRow.id,
          name: itemRow.name,
          quantity: itemRow.quantity,
          premise_id: itemRow.premise_id,
          period_list:
            Model.Pricing.period_list_of_json(
              Melange_json.of_string(itemRow.period_list),
            ),
        }: Model.InventoryItem.t),
        Array.of_list(inventoryRows),
      );
    let config: Model.t = {
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
    pathname: serverPathname,
    search: serverSearch,
  } = context;
  let app =
    <UniversalRouter
      router=Routes.router
      state=store
      basePath
      serverPathname
      serverSearch
    />;
  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~basePath,
      ~pathname=serverPathname,
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
