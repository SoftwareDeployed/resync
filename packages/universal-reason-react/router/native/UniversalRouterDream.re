open Lwt.Syntax;

let requestPath = request => Dream.target(request) |> Dream.split_target |> fst;

let requestSearch = request => Dream.target(request) |> Dream.split_target |> snd;

let requestSearchParams = request => requestSearch(request) |> UniversalRouter.SearchParams.parse;

type serverContext('state) = {
  request: Dream.request,
  basePath: string,
  pathname: string,
  search: string,
  searchParams: UniversalRouter.SearchParams.t,
  params: UniversalRouter.Params.t,
  matchResult: UniversalRouter.matchResult('state),
};

type serverStateResult('state) =
  | State('state)
  | NotFound
  | Redirect({location: string, permanent: bool});

type resolvedServerState('state) = {
   context: serverContext('state),
  state: 'state,
};

type loadedServerState('state) =
  | ReadyState(resolvedServerState('state))
  | ServerNotFound
  | ServerRedirect({location: string, permanent: bool});

type app('state) = {
   router: UniversalRouter.t('state),
   getServerState: serverContext('state) => Lwt.t(serverStateResult('state)),
   render: (~context: serverContext('state), ~serverState: 'state, unit) => React.element,
  };

let app = (~router, ~getServerState, ~render, ()) => {
  router,
  getServerState,
  render,
};

let resolvedContext = resolvedServerState => resolvedServerState.context;
let resolvedState = resolvedServerState => resolvedServerState.state;

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

let matchRequest = (~router, ~basePath, request) =>
  UniversalRouter.matchMountedPath(
    ~router,
    ~basePath,
    ~pathname=requestPath(request),
    ~searchParams=requestSearchParams(request),
    (),
  );

let loadServerState = (~router, ~basePath, ~request, ~getServerState) => {
  let search = requestSearch(request);
  let searchParams = requestSearchParams(request);

  switch (matchRequest(~router, ~basePath, request)) {
  | None => Lwt.return(ServerNotFound)
  | Some(matchResult) =>
    let context = {
      request,
      basePath,
      pathname: matchResult.pathname,
      search,
      searchParams,
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

let renderDocument = (~router, ~basePath, ~serializedState="", ~serializedQueries="", ~state=?, ~children, request) => {
  switch (matchRequest(~router, ~basePath, request)) {
  | Some(matchResult) =>
    UniversalRouter.renderDocument(
      ~router,
      ~children,
      ~basePath,
      ~pathname=matchResult.pathname,
      ~search=requestSearch(request),
      ~serializedState,
      ~serializedQueries,
      ~state,
      (),
    )
  | None =>
    UniversalRouter.renderDocument(
      ~router,
      ~children,
      ~basePath,
      ~pathname="/",
      ~search=requestSearch(request),
      ~serializedState,
      ~serializedQueries,
      ~state,
      (),
    )
  };
};

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
  | [basePath, ...remainingRoots] => {
      let* serverState =
        loadServerState(
          ~router=app.router,
          ~basePath,
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
  let candidateRoots = UniversalRouter.candidateBasePaths(requestPath);
  respondCandidateRoots(~app, ~request, candidateRoots);
};
