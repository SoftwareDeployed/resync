// UseMutation.re - Universal React hook for mutations
//
// Store-scoped API:
//   module Mutations = {
//     module AddTodo = {
//       type params = todo;
//       type nonrec action = action;
//       let toAction = params => AddTodo(params);
//     };
//   };
//   let addTodo = TodoStore.Hooks.useMutation((module TodoStore.Mutations.AddTodo), ());
//   addTodo.mutate(todo);
//   let addTodo = TodoStore.Hooks.useMutationFn((module TodoStore.Mutations.AddTodo), ());
//   addTodo(todo);
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

[@platform js]
external stringOfJsValue: 'a => string = "String";

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
  let mountedRef = React.useRef(true);
  let onDispatchRef = React.useRef(onDispatch);
  onDispatchRef.current = onDispatch;

  React.useEffect0(() => {
    mountedRef.current = true;
    Some(() => {
      mountedRef.current = false;
    });
  });

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
      | _ => stringOfJsValue(error)
      }
    };
  };

  let beginMutation = () => {
    pendingCountRef.current = pendingCountRef.current + 1;
    if (mountedRef.current) {
      setLoading(_ => true);
      setError(_ => None);
    };
  };

  let finishMutation = () => {
    let nextCount = pendingCountRef.current - 1;
    pendingCountRef.current = if (nextCount < 0) {0} else {nextCount};
    if (mountedRef.current) {
      setLoading(_ => pendingCountRef.current > 0);
    };
  };

  // Client: Delegate to the dispatch callback supplied by the runtime.
  let dispatch =
    React.useMemo1(
      () =>
        (params: p): Js.Promise.t(unit) => {
          beginMutation();
          let promise =
            try({
              onDispatchRef.current(params)
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
               if (mountedRef.current) {
                 setError(_ => Some(errorMessage(error)));
               };
               Js.Promise.reject(Js.Exn.anyToExnInternal(error));
             });
        },
      [||],
    );

  {dispatch, mutate: dispatch, loading, error};
};

[@platform js]
let makeFn =
    (
      type p,
      module M: MutationModule with type params = p,
      ~onDispatch: p => Js.Promise.t(unit),
      (),
    ) => {
  let onDispatchRef = React.useRef(onDispatch);
  onDispatchRef.current = onDispatch;

  React.useMemo1(
    () =>
      (params: p): Js.Promise.t(unit) =>
        try({
          onDispatchRef.current(params)
        }) {
        | error => Js.Promise.reject(error)
        },
    [||],
  );
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

[@platform native]
let makeFn =
    (
      type p,
      module M: MutationModuleNative with type params = p,
      ~onDispatch: option(p => Js.Promise.t(unit))=?,
      (),
    ) => {
  (_params: p): Js.Promise.t(unit) => Js.Promise.resolve();
};
