open Lwt.Syntax;

let getServerState =
    (_context: UniversalRouterDream.serverContext(unit)) => {
  Lwt.return(UniversalRouterDream.State());
};

let render = (~context, ~serverState: unit, ()) => {
  let UniversalRouterDream.{basePath, pathname: serverPathname, search: serverSearch} = context;
  let app =
    <UniversalRouter
      router=Routes.router
      state=serverState
      basePath
      serverPathname
      serverSearch
    />;
  UniversalRouter.renderDocument(
    ~router=Routes.router,
    ~children=app,
    ~basePath,
    ~pathname=serverPathname,
    ~search=serverSearch,
    ~serializedState="null",
    ~state=serverState,
    (),
  );
};

let app = UniversalRouterDream.app(~router=Routes.router, ~getServerState, ~render, ());
