// UseMutation.re - Universal React hook for mutations
//
// Store-scoped API:
//   module AddTodoMutation = {
//     include RealtimeSchema.Mutations.AddTodo;
//     type action = TodoStore.action;
//   };
//   let {mutate} = TodoStore.useMutation((module AddTodoMutation), ());
//   mutate({id: UUID.make(), list_id: "abc", text: "Hello"});
//
// On client (JS): Calls the provided dispatch callback. Store-scoped synced
// runtimes resolve this promise after the server acknowledges the mutation.
// On server (native): No-op for SSR

open QueryRegistryTypes;

// Mutation result type
type mutation_result('params) = {
  dispatch: 'params => Js.Promise.t(unit),
  mutate: 'params => Js.Promise.t(unit),
  loading: bool,
  error: option(string),
};

// Main useMutation hook - JS version (client-side)
[@platform js]
let make =
    (
      type p,
      module M: MutationModule with type params = p,
      ~onDispatch: p => Js.Promise.t(unit),
      (),
    ) => {
  let (loading, setLoading) = React.useState(() => false);
  let (error, setError) = React.useState(() => None);
  let pendingCountRef = React.useRef(0);

  let errorMessage = (error: Js.Promise.error) => {
    let exn = Js.Exn.anyToExnInternal(error);
    switch (Js.Exn.asJsExn(exn)) {
    | Some(jsError) =>
      switch (Js.Exn.message(jsError)) {
      | Some(message) => message
      | None => "Mutation failed"
      }
    | None =>
      switch (exn) {
      | Failure(message) => message
      | _ => "Mutation failed"
      }
    };
  };

  let beginMutation = () => {
    pendingCountRef.current = pendingCountRef.current + 1;
    setLoading(_ => true);
    setError(_ => None);
  };

  let finishMutation = () => {
    let nextCount = pendingCountRef.current - 1;
    pendingCountRef.current = if (nextCount < 0) {0} else {nextCount};
    setLoading(_ => pendingCountRef.current > 0);
  };

  // Client: Delegate to the dispatch callback supplied by the runtime.
  let dispatch = (params: p): Js.Promise.t(unit) => {
    beginMutation();
    let promise =
      try({
        onDispatch(params)
      }) {
      | error => Js.Promise.reject(error)
      };

    promise
    |> Js.Promise.then_(_ => {
         finishMutation();
         Js.Promise.resolve();
       })
    |> Js.Promise.catch(error => {
         finishMutation();
         setError(_ => Some(errorMessage(error)));
         Js.Promise.reject(Js.Exn.anyToExnInternal(error));
       });
  };

  {dispatch, mutate: dispatch, loading, error};
};

// Main useMutation hook - Native version (server-side)
[@platform native]
let make =
    (
      type p,
      module M: MutationModuleNative with type params = p,
      ~onDispatch: option(p => Js.Promise.t(unit))=?,
      (),
    ) => {
  // Server: Mutations are not dispatched during SSR
  let dispatch = (_params: p): Js.Promise.t(unit) => Js.Promise.resolve(());
  {dispatch, mutate: dispatch, loading: false, error: None};
};
