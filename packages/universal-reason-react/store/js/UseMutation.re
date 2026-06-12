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
// On client (JS): Dispatches mutation through the store's dispatch function
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
      ~onDispatch: p => unit,
      (),
    ) => {
  // Client: Delegate to the store's dispatch function
  let dispatch = (params: p): Js.Promise.t(unit) => {
    try({
      onDispatch(params);
      Js.Promise.resolve(());
    }) {
    | error => Js.Promise.reject(error)
    };
  };

  {dispatch, mutate: dispatch, loading: false, error: None};
};

// Main useMutation hook - Native version (server-side)
[@platform native]
let make =
    (
      type p,
      module M: MutationModuleNative with type params = p,
      ~onDispatch: option(p => unit)=?,
      (),
    ) => {
  // Server: Mutations are not dispatched during SSR
  let dispatch = (_params: p): Js.Promise.t(unit) => Js.Promise.resolve(());
  {dispatch, mutate: dispatch, loading: false, error: None};
};
