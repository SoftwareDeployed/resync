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

type store = {
  state: state,
  completed_count: int,
  total_count: int,
};

let storeName = "todo.simple";

let emptyState: state = {
  todos: [||],
  updated_at: 0.0,
};

let nextTodoId = (todos: array(todo)) => {
  let next =
    Array.to_list(todos)
    |> List.fold_left(
         (current, item: todo) =>
           switch (
             try (Some(int_of_string(item.id))) {
             | Failure(_) => None
             }
           ) {
           | Some(id) when id >= current => id + 1
           | _ => current
           },
         1,
       );
  string_of_int(next);
};

let completedCount = todos =>
  todos->Js.Array.filter(~f=(item: todo) => item.completed)->Array.length;

let action_to_json = action =>
  switch (action) {
  | AddTodo(todo) =>
    StoreJson.parse(
      "{\"kind\":\"add_todo\",\"todo\":" ++ StoreJson.stringify(todo_to_json, todo) ++ "}",
    )
  | SetTodoCompleted(input) =>
    StoreJson.parse(
      "{\"kind\":\"set_todo_completed\",\"id\":"
      ++ string_to_json(input.id)->Melange_json.to_string
      ++ ",\"completed\":"
      ++ bool_to_json(input.completed)->Melange_json.to_string
      ++ "}",
    )
  | RemoveTodo(id) =>
    StoreJson.parse(
      "{\"kind\":\"remove_todo\",\"id\":"
      ++ string_to_json(id)->Melange_json.to_string
      ++ "}",
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

let reduce = (~state: state, ~action: action) => {
  let updated_at = Js.Date.now();
  switch (action) {
  | AddTodo(todo) => {
      todos: StoreCrud.upsert(~getId=(item: todo) => item.id, state.todos, todo),
      updated_at,
    }
  | SetTodoCompleted(input) => {
      todos:
        Js.Array.map(
          ~f=(item: todo) =>
            item.id == input.id ? {...item, completed: input.completed} : item,
          state.todos,
        ),
      updated_at,
    }
  | RemoveTodo(id) => {
      todos: StoreCrud.remove(~getId=(item: todo) => item.id, state.todos, id),
      updated_at,
    }
  };
};

/* ============================================================================
   Pipeline Builder API
   ============================================================================ */

module StoreDef =
  StoreBuilder.Local.Define({
    type nonrec state = state;
    type nonrec action = action;
    type nonrec store = store;

    let input =
      StoreBuilder.make()
      |> StoreBuilder.withSchema({
           emptyState,
           reduce,
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
         );
  });

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
