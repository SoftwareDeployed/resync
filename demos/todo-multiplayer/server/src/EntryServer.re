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
      let* listInfo = Dream.sql(request, Database.Todo.get_list_info(listId));
      switch (listInfo) {
      | None => Lwt.return(UniversalRouterDream.NotFound)
      | Some(list) =>
        let* todos = Dream.sql(request, Database.Todo.get_list(listId));
        let config: Model.t = {
          todos,
          list: Some(list),
        };
        let store = TodoStore.createStore(config);
        Lwt.return(UniversalRouterDream.State(store));
      };
    };
  };
};

let render = (~context, ~serverState: TodoStore.t, ()) => {
  let store = serverState;
  let serializedState = TodoStore.serializeState(serverState.config);
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
