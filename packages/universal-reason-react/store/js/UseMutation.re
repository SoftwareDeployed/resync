// UseMutation.re - Universal React hook for mutations
//
// API: let {dispatch} = UseMutation.make((module RealtimeSchema.Mutations.AddTodo));
// dispatch({id: UUID.make(), list_id: "abc", text: "Hello"})
//   |> Js.Promise.then_(_ => Js.Promise.resolve(()));
//
// On client (JS): Dispatches mutation over WebSocket
// On server (native): No-op for SSR

open QueryRegistryTypes;

// Mutation result type
type mutation_result('params) = {
  dispatch: 'params => Js.Promise.t(unit),
  loading: bool,
  error: option(string),
};

// Main useMutation hook
let make =
    (
      type p,
      module M: MutationModule with type params = p,
      (),
    ) => {
  switch%platform (Runtime.platform) {
  | Client =>
    // Client: Return dispatch function that sends mutation over WebSocket
    let (loading, setLoading) = React.useState(() => false);
    let (error, setError) = React.useState(() => None);

    let dispatch = (params: p): Js.Promise.t(unit) => {
      setLoading(_ => true);
      setError(_ => None);

      let actionId = UUID.make();
      let actionJson = M.encodeParams(params);
      let frame =
        RealtimeClient.mutationFrameString(
          actionId,
          StoreJson.stringify(json => json, actionJson),
        );

      // Get the query cache's WebSocket connection and send the mutation frame
      let cache = UseQuery.getQueryCache();
      let handle =
        RealtimeClient.Socket.subscribeSynced(
          ~subscription=M.name,
          ~updatedAt=0.0,
          ~onPatch=(~payload as _, ~timestamp as _) => (),
          ~onSnapshot=_payload => (),
          ~onAck=(_actionId, _status, _error) => {
            setLoading(_ => false);
          },
          ~onOpen=() => (),
          ~onClose=() => (),
          ~eventUrl=cache.eventUrl,
          ~baseUrl=cache.baseUrl,
          (),
        );

      let _ = RealtimeClient.Socket.sendFrame(~handle, ~frame);
      RealtimeClient.Socket.disposeHandle(handle);

      Js.Promise.resolve(());
    };

    {dispatch, loading, error};

  | Server =>
    // Server: Mutations are not dispatched during SSR
    let dispatch = (_params: p): Js.Promise.t(unit) => Js.Promise.resolve(());
    {dispatch, loading: false, error: None};
  };
};