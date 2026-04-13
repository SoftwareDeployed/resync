open Lwt.Syntax;

let isHexDigit = (char: char) =>
  switch (char) {
  | '0' .. '9'
  | 'a' .. 'f'
  | 'A' .. 'F' => true
  | _ => false
  };

let isUuid = (value: string) => {
  let length = String.length(value);
  if (Int.equal(length, 36)) {
    let rec loop = index =>
      if (Int.equal(index, length)) {
        true;
      } else {
        let char = String.get(value, index);
        if (
          Int.equal(index, 8)
          || Int.equal(index, 13)
          || Int.equal(index, 18)
          || Int.equal(index, 23)
        ) {
          Char.equal(char, '-') && loop(index + 1);
        } else {
          isHexDigit(char) && loop(index + 1);
        };
      };
    loop(0);
  } else {
    false;
  };
};

let getServerState = (context: UniversalRouterDream.serverContext(TodoStore.t)) => {
  let UniversalRouterDream.{basePath, request} = context;
  if (String.length(basePath) <= 1) {
    Lwt.return(UniversalRouterDream.NotFound);
  } else {
    let listId = String.sub(basePath, 1, String.length(basePath) - 1);
    if (!isUuid(listId)) {
      Lwt.return(UniversalRouterDream.NotFound);
    } else {
      let* listInfo =
        Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
          RealtimeSchema.Queries.GetListInfo.find_opt(
            (module Db),
            RealtimeSchema.Queries.GetListInfo.caqti_type,
            listId,
          )
        );
      switch (listInfo) {
      | None => Lwt.return(UniversalRouterDream.NotFound)
      | Some(listRow) =>
        let* todoRows =
          Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
            RealtimeSchema.Queries.GetList.collect(
              (module Db),
              RealtimeSchema.Queries.GetList.caqti_type,
              listId,
            )
          );
        let todos =
          List.map(
            (row: RealtimeSchema.Queries.GetList.row) =>
              ({Model.Todo.id: row.id, list_id: row.list_id, text: row.text, completed: row.completed}: Model.Todo.t),
            todoRows,
          );
        let list =
          Some((
            {Model.TodoList.id: listRow.id, name: listRow.name, updated_at: listRow.updated_at}: Model.TodoList.t
          ));
        let config: Model.t = {
          todos: Array.of_list(todos),
          list,
        };
        let store = TodoStore.createStore(config);
        Lwt.return(UniversalRouterDream.State(store));
      };
    };
  };
};

let render = (~context, ~serverState: TodoStore.t, ()) => {
  let store = serverState;
  let serializedState = TodoStore.serializeState(serverState.state);
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
  <TodoStore.Context.Provider value=store>
    document
  </TodoStore.Context.Provider>;
};

let app =
  UniversalRouterDream.app(
    ~router=Routes.router,
    ~getServerState,
    ~render,
    (),
  );
