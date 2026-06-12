open Melange_json.Primitives;

[@deriving json]
type todo = {
  id: string,
  text: string,
  completed: bool,
};

[@deriving json]
type state = {
  todos: array(todo),
  updated_at: float,
};

type set_completed = {
  id: string,
  completed: bool,
};

type action =
  | AddTodo(todo)
  | SetTodoCompleted(set_completed)
  | RemoveTodo(string);

module Mutations = {
  module AddTodo = {
    type params = todo;
    type nonrec action = action;
    let toAction = params => AddTodo(params);
  };

  module SetTodoCompleted = {
    type params = set_completed;
    type nonrec action = action;
    let toAction = params => SetTodoCompleted(params);
  };

  module RemoveTodo = {
    type params = string;
    type nonrec action = action;
    let toAction = id => RemoveTodo(id);
  };
};

type store = {
  state: state,
  completed_count: int,
  total_count: int,
};

let storeName = "todo.simple";

let nextTodoId = (todos: array(todo)) => {
  let rec loop = (index, current) =>
    if (index >= Array.length(todos)) {
      current;
    } else {
      let item = todos[index];
      let next =
        switch (
          try (Some(int_of_string(item.id))) {
          | Failure(_) => None
          }
        ) {
        | Some(id) when id >= current => id + 1
        | _ => current
        };
      loop(index + 1, next);
    };

  string_of_int(loop(0, 1));
};

let completedCount = todos =>
  todos->Js.Array.filter(~f=(item: todo) => item.completed)->Array.length;

let actionJson = (~kind, ~fill) => {
  StoreJson.Object.make(dict => {
    StoreJson.Object.setString(dict, "kind", kind);
    fill(dict);
  });
};

let action_to_json = action =>
  switch (action) {
  | AddTodo(todo) =>
    actionJson(
      ~kind="add_todo",
      ~fill=dict => StoreJson.Object.setJson(dict, "todo", todo_to_json(todo)),
    )
  | SetTodoCompleted(input) =>
    actionJson(
      ~kind="set_todo_completed",
      ~fill=dict => {
        StoreJson.Object.setString(dict, "id", input.id);
        StoreJson.Object.setBool(dict, "completed", input.completed);
      },
    )
  | RemoveTodo(id) =>
    actionJson(
      ~kind="remove_todo",
      ~fill=dict => StoreJson.Object.setString(dict, "id", id),
    )
  };

let action_of_json = json => {
  let kind =
    StoreJson.requiredField(
      ~json,
      ~fieldName="kind",
      ~decode=string_of_json,
    );
  switch (kind) {
  | "add_todo" =>
    AddTodo(
      StoreJson.requiredField(~json, ~fieldName="todo", ~decode=todo_of_json),
    )
  | "set_todo_completed" =>
    SetTodoCompleted({
      id: StoreJson.requiredField(~json, ~fieldName="id", ~decode=string_of_json),
      completed:
        StoreJson.requiredField(
          ~json,
          ~fieldName="completed",
          ~decode=bool_of_json,
        ),
    })
  | _ =>
    RemoveTodo(
      StoreJson.requiredField(~json, ~fieldName="id", ~decode=string_of_json),
    )
  };
};

/* ============================================================================
   Pipeline Builder API
   ============================================================================ */

module StoreDef =
  (val StoreBuilder.buildLocal(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState: {
           todos: [||],
           updated_at: 0.0,
         },
         reduce: (~state: state, ~action: action) => {
           let updated_at = Js.Date.now();
           switch (action) {
           | AddTodo(todo) => {
               todos:
                 StoreCrud.upsert(
                   ~getId=(item: todo) => item.id,
                   state.todos,
                   todo,
                 ),
               updated_at,
             }
           | SetTodoCompleted(input) => {
               todos:
                 state.todos->Js.Array.map(
                   ~f=(item: todo) =>
                     item.id == input.id ? {...item, completed: input.completed} : item,
                 ),
               updated_at,
             }
           | RemoveTodo(id) => {
               todos:
                 StoreCrud.remove(
                   ~getId=(item: todo) => item.id,
                   state.todos,
                   id,
                 ),
               updated_at,
             }
           };
         },
         makeStore:
           (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
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
         },
       })
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withLocalPersistence(
         ~storeName,
         ~scopeKeyOfState = (_state: state) => "default",
         ~timestampOfState = (state: state) => state.updated_at,
         ~stateElementId=None,
         (),
       ),
  ));

include (
  StoreDef:
    StoreBuilder.Runtime.Exports
      with type state := state
      and type action := action
      and type t := store
);

type t = store;

module Context = StoreDef.Context;

let addTodo = (store: t, text: string) => {
  let _ = store;
  dispatch(AddTodo({
    id: nextTodoId(store.state.todos),
    text,
    completed: false,
  }));
};

let toggleTodo = (store: t, id: string) =>
  switch (Js.Array.find(~f=(item: todo) => item.id == id, store.state.todos)) {
  | Some(todo) => dispatch(SetTodoCompleted({id, completed: !todo.completed}))
  | None => ()
  };

let removeTodo = (_store: t, id: string) => dispatch(RemoveTodo(id));
