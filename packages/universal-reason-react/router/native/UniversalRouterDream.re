let requestPath = request => Dream.target(request) |> Dream.split_target |> fst;

let requestSearch = request => Dream.target(request) |> Dream.split_target |> snd;

let requestQuery = request => Dream.all_queries(request) |> UniversalRouter.Query.ofList;

let matchRequest = (~router, ~routeRoot, request) =>
  UniversalRouter.matchMountedPath(
    ~router,
    ~routeRoot,
    ~path=requestPath(request),
    ~query=requestQuery(request),
    (),
  );

let renderDocument = (~router, ~routeRoot, ~serializedState="", ~children, request) =>
  UniversalRouter.renderDocument(
    ~router,
    ~children,
    ~routeRoot,
    ~path=requestPath(request),
    ~search=requestSearch(request),
    ~serializedState,
    (),
  );
