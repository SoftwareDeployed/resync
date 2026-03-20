type serverContext('state) = {
  request: Dream.request,
  basePath: string,
  path: string,
  search: string,
  query: UniversalRouter.Query.t,
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

let app:
  (
    ~router: UniversalRouter.t('state),
    ~getServerState: serverContext('state) => Lwt.t(serverStateResult('state)),
    ~render: (~context: serverContext('state), ~serverState: 'state, unit) => React.element,
    unit,
  ) =>
  app('state);

let handler: (~app: app('state), Dream.request) => Lwt.t(Dream.response);
