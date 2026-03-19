open Lwt.Syntax;

let requestPath = request => Dream.target(request) |> Dream.split_target |> fst;

let requestSearch = request => Dream.target(request) |> Dream.split_target |> snd;

let requestQuery = request => Dream.all_queries(request) |> UniversalRouter.Query.ofList;

type serverContext = {
  request: Dream.request,
  routeRoot: string,
  path: string,
  search: string,
  query: UniversalRouter.Query.t,
  params: UniversalRouter.Params.t,
  matchResult: UniversalRouter.matchResult,
};

type serverStateResult('state) =
  | State('state)
  | NotFound
  | Redirect({location: string, permanent: bool});

type resolvedServerState('state) = {
  context: serverContext,
  state: 'state,
};

type loadedServerState('state) =
  | ReadyState(resolvedServerState('state))
  | ServerNotFound
  | ServerRedirect({location: string, permanent: bool});

type app('state) = {
  router: UniversalRouter.t,
  getServerState: serverContext => Lwt.t(serverStateResult('state)),
  render: (~context: serverContext, ~serverState: 'state, unit) => React.element,
};

let app = (~router, ~getServerState, ~render, ()) => {
  router,
  getServerState,
  render,
};

let resolvedContext = resolvedServerState => resolvedServerState.context;
let resolvedState = resolvedServerState => resolvedServerState.state;

let contextRouteRoot = context => context.routeRoot;
let contextPath = context => context.path;
let contextSearch = context => context.search;
let contextQuery = context => context.query;
let contextParams = context => context.params;
let contextMatchResult = context => context.matchResult;
let contextRequest = context => context.request;

let streamReactApp = (responseStream, reactElement) => {
  let* () = Dream.write(responseStream, "<!DOCTYPE html>");
  let* (stream, _abort) = ReactDOM.renderToStream(reactElement);
  let* () =
    Lwt_stream.iter_s(
      chunk => {
        let* () = Dream.write(responseStream, chunk);
        Dream.flush(responseStream);
      },
      stream,
    );
  Lwt.return(());
};

let matchRequest = (~router, ~routeRoot, request) =>
  UniversalRouter.matchMountedPath(
    ~router,
    ~routeRoot,
    ~path=requestPath(request),
    ~query=requestQuery(request),
    (),
  );

let loadServerState = (~router, ~routeRoot, ~request, ~getServerState) => {
  let path = requestPath(request);
  let search = requestSearch(request);
  let query = requestQuery(request);

  switch (matchRequest(~router, ~routeRoot, request)) {
  | None => Lwt.return(ServerNotFound)
  | Some(matchResult) =>
    let context = {
      request,
      routeRoot,
      path,
      search,
      query,
      params: matchResult.params,
      matchResult,
    };

    let* result = getServerState(context);
    switch (result) {
    | State(state) => Lwt.return(ReadyState({context, state}))
    | NotFound => Lwt.return(ServerNotFound)
    | Redirect({location, permanent}) =>
      Lwt.return(ServerRedirect({location, permanent}))
    };
  };
};

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

let renderResolved = (~app, resolvedServerState) =>
  app.render(
    ~context=resolvedContext(resolvedServerState),
    ~serverState=resolvedState(resolvedServerState),
    (),
  );

let respondResolved = (~app, resolvedServerState) =>
  Dream.stream(
    ~headers=[("Content-Type", "text/html")],
    (responseStream =>
      streamReactApp(responseStream, renderResolved(~app, resolvedServerState))),
  );

let redirectResponse = (~request, ~location, ~permanent) => {
  let status = if (permanent) { `Moved_Permanently } else { `See_Other };
  Dream.redirect(~status=status, request, location);
};

let rec respondCandidateRoots = (~app, ~request, candidateRoots) =>
  switch (candidateRoots) {
  | [] => Dream.empty(`Not_Found)
  | [routeRoot, ...remainingRoots] => {
      let* serverState =
        loadServerState(
          ~router=app.router,
          ~routeRoot,
          ~request,
          ~getServerState=app.getServerState,
        );
      switch (serverState) {
      | ReadyState(resolvedServerState) => respondResolved(~app, resolvedServerState)
      | ServerRedirect({location, permanent}) =>
        redirectResponse(~request, ~location, ~permanent)
      | ServerNotFound =>
        respondCandidateRoots(~app, ~request, remainingRoots)
      };
    }
  };

let handler = (~app, request) => {
  let requestPath = requestPath(request);
  let candidateRoots = UniversalRouter.candidateRouteRoots(requestPath);
  respondCandidateRoots(~app, ~request, candidateRoots);
};
