open Lwt.Syntax;

let getServerState = (context: UniversalRouterDream.serverContext(Store.t)) => {
  let UniversalRouterDream.{ basePath, request, params } = context;
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
    let* order =
      switch (String.length(context.pathname) >= 7
             && String.sub(context.pathname, 0, 7) == "/order/") {
      | true =>
        switch (UniversalRouter.Params.find("id", params)) {
        | Some(orderId) when StoreChannel.isValidUuid(orderId) =>
          let* orderRow =
            Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
              RealtimeSchema.Queries.GetOrder.find_opt(
                (module Db),
                RealtimeSchema.Queries.GetOrder.caqti_type,
                orderId,
              )
            );
          let* itemRows =
            Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
              RealtimeSchema.Queries.GetOrderItems.collect(
                (module Db),
                RealtimeSchema.Queries.GetOrderItems.caqti_type,
                orderId,
              )
            );
          switch (orderRow) {
          | Some((orderRow: RealtimeSchema.Queries.GetOrder.row)) =>
            let items =
              Array.map(
                (itemRow: RealtimeSchema.Queries.GetOrderItems.row) => ({
                  Model.OrderItem.inventory_id: itemRow.inventory_id,
                  name: itemRow.name,
                  quantity: itemRow.quantity,
                  unit_label: itemRow.unit_label,
                  unit_price_cents: Int64.to_int(itemRow.unit_price_cents),
                }: Model.OrderItem.t),
                Array.of_list(itemRows),
              );
            Lwt.return(
              Some(
                ({
                  Model.Order.id: orderRow.id,
                  premise_id: orderRow.premise_id,
                  customer_name: orderRow.customer_name,
                  customer_email: orderRow.customer_email,
                  start_date: orderRow.start_date,
                  end_date: orderRow.end_date,
                  status: orderRow.status,
                  created_at: Js.Date.fromFloat(orderRow.created_at),
                  items,
                }: Model.Order.t),
              ),
            )
          | None => Lwt.return(None)
          }
        | _ => Lwt.return(None)
        }
      | false => Lwt.return(None)
      };
    let config: Model.t = {
      inventory,
      premise: Some(premise),
      order,
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
