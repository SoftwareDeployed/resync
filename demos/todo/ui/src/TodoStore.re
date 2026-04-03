open Melange_json.Primitives;

[@deriving json]
type todo = {
  id: string,
  text: string,
  completed: bool,
};

[@deriving json]
type state = {todos: array(todo)};

[@deriving json]
type payload = state;

type store = {
  state: state,
  completed_count: int,
  total_count: int,
};

let storageKey = "todo.simple";
let emptyState: state = {todos: [||]};
let emptyPayload: payload = emptyState;

let nextTodoId = (todos: array(todo)) => {
  let next =
    Array.to_list(todos)
    |> List.fold_left(
         (next, todo: todo) =>
           switch (
             try(Some(int_of_string(todo.id))) {
             | Failure(_) => None
             }
           ) {
           | Some(id) when id >= next => id + 1
           | _ => next
           },
         1,
       );
  string_of_int(next);
};

let completedCount = (todos: array(todo)) =>
  todos->Js.Array.filter(~f=(todo: todo) => todo.completed)->Array.length;

module Local = StoreLocal.Make({
  type nonrec state = state;
  type nonrec payload = payload;

  module Adapter = StoreLocal.LocalStorageAdapter;

  let storageKeyOfState = (_state: state) => storageKey;
  let payloadOfState = (state: state): payload => state;
  let stateOfPayload = (payload: payload): state => payload;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
});

module Layered = StoreBuilder.Layered.Make({
  type nonrec state = state;
  type nonrec payload = payload;
  type nonrec store = store;

  let emptyStore: store = {
    state: emptyState,
    completed_count: 0,
    total_count: 0,
  };
  let emptyPayload = emptyPayload;
  let stateElementId = "initial-store";
  let payloadOfState = (state: state): payload => state;
  let stateOfPayload = (payload: payload): state => payload;
  let state_of_json = state_of_json;
  let state_to_json = state_to_json;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
  let clientLayers = [|Local.hooks|];

  let makeStore =
      (
        ~state: state,
        ~payload: payload,
        ~derive: option(Tilia.Core.deriver(store))=?,
        (),
      ):
      store => {
    let _ = payload;
    {
      state:
        StoreBuilder.current(
          ~derive?,
          ~client=state,
          ~server=() => state,
          (),
        ),
      completed_count:
        StoreBuilder.derived(
          ~derive?,
          ~client=store => completedCount(store.state.todos),
          ~server=() => completedCount(state.todos),
          (),
        ),
      total_count:
        StoreBuilder.derived(
          ~derive?,
          ~client=store => Array.length(store.state.todos),
          ~server=() => Array.length(state.todos),
          (),
        ),
    };
  };
});

include (
  Layered:
    StoreBuilder.Layered.Exports
      with type state := state
      and type payload := payload
      and type t := store
);

type t = store;

module Context = Layered.Context;

let updateTodos = (~store: t, reducer: array(todo) => array(todo)) => {
  Local.set(({todos: reducer(store.state.todos)}: state));
};

let addTodo = (store: t, text: string) => {
  let newTodo: todo = {
    id: nextTodoId(store.state.todos),
    text,
    completed: false,
  };
  updateTodos(~store, todos =>
    StoreCrud.upsert(~getId=(todo: todo) => todo.id, todos, newTodo)
  );
};

let toggleTodo = (store: t, id: string) =>
  updateTodos(~store, todos =>
    Js.Array.map(
      ~f=(todo: todo) => todo.id == id ? {...todo, completed: !todo.completed} : todo,
      todos,
    )
  );

let removeTodo = (store: t, id: string) =>
  updateTodos(~store, todos =>
    StoreCrud.remove(~getId=(todo: todo) => todo.id, todos, id)
  );
