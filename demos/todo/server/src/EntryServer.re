open Lwt.Syntax;

let getServerState =
    (_context: UniversalRouterDream.serverContext(TodoStore.t)) => {
  let config: TodoStore.config = {
    todos: [
      {
        id: "1",
        text: "Learn ReasonML",
        completed: false,
      },
      {
        id: "2",
        text: "Build an app",
        completed: false,
      },
      {
        id: "3",
        text: "Deploy to production",
        completed: false,
      },
    ],
  };

  let store = TodoStore.createStore(config);
  Lwt.return(UniversalRouterDream.State(store));
};

let render = (~context, ~serverState: TodoStore.t, ()) => {
  let UniversalRouterDream.{
    basePath,
    path: serverPath,
    search: serverSearch,
  } = context;

  let serializedState =
    TodoStore.serializeState({ todos: serverState.todos });

  let app =
    <UniversalRouter
      router=Routes.router
      state=serverState
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
      ~state=serverState,
      (),
    );

  <TodoStore.Context.Provider value=serverState>
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
