open Melange_json.Primitives;

[@deriving json]
type todo = {
  id: string,
  text: string,
  completed: bool,
};

[@deriving json]
type config = {todos: array(todo)};

[@deriving json]
type payload = {todos: array(todo)};

type patch = unit;
type subscription = unit;

type store = {
  todos: array(todo),
  completed_count: int,
  total_count: int,
};

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

let todosSourceRef: ref(option(StoreSource.t(array(todo)))) = ref(None);

let completedCount = (todos: array(todo)) =>
  todos->Js.Array.filter(~f=(todo: todo) => todo.completed)->Array.length;

module Runtime =
  StoreBuilder.Runtime.Make({
    type nonrec config = config;
    type nonrec patch = patch;
    type nonrec payload = payload;
    type nonrec store = store;
    type nonrec subscription = subscription;

    let emptyStore: store = {
      todos: [||],
      completed_count: 0,
      total_count: 0,
    };
    let stateElementId = "initial-store";

    let payloadOfConfig = (config: config): payload => {
      todos: config.todos,
    };
    let configOfPayload = (payload: payload): config => {
      todos: payload.todos,
    };

    let makeStore =
        (
          ~config: config,
          ~payload: payload,
          ~derive: option(Tilia.Core.deriver(store))=?,
          (),
        )
        : store => {
      let todosSource = StoreSource.make(payload.todos);
      todosSourceRef := Some(todosSource);

      {
        todos: todosSource.value,
        completed_count:
          StoreBuilder.derived(
            ~derive?,
            ~client=store => completedCount(store.todos),
            ~server=() => completedCount(config.todos),
            (),
          ),
        total_count:
          StoreBuilder.derived(
            ~derive?,
            ~client=store => Array.length(store.todos),
            ~server=() => Array.length(config.todos),
            (),
          ),
      };
    };

    let config_of_json = config_of_json;
    let config_to_json = config_to_json;
    let payload_of_json = payload_of_json;
    let payload_to_json = payload_to_json;
    let decodePatch = _json => None;
    let subscriptionOfConfig = _config => None;
    let encodeSubscription = _subscription => "";
    let updatedAtOf = _config => 0.0;
    let updateOfPatch = (_patch: patch, config) => config;
    let eventUrl = "";
    let baseUrl = "";
  });

include (
  Runtime:
    StoreBuilder.Runtime.Exports
      with type config := config
      and type payload := payload
      and type t := store
);

type t = store;

module Context = Runtime.Context;

let log = (label: string, value: 'a) =>
  switch%platform (Runtime.platform) {
  | Client => Js.log2(label, value)
  | Server => ()
  };

let logTodoIds = (label: string, todos: array(todo)) => {
  let summary =
    Array.to_list(todos)
    |> List.map((todo: todo) => todo.id ++ ": " ++ todo.text)
    |> String.concat(", ");
  log(label ++ " (" ++ string_of_int(Array.length(todos)) ++ "): ", summary);
};

let hydrateStore = () => Runtime.hydrateStore();

let hydrateStoreWithLogs = () => {
  let store = hydrateStore();
  logTodoIds("[todo] hydrated todos", store.todos);
  store;
};

let serializeState = (config: config) => Runtime.serializeState(config);

let updateTodos = (~store: t, reducer) => {
  let source =
    switch (todosSourceRef.contents) {
    | Some(todosSource) => todosSource
    | None =>
      let todosSource = StoreSource.make(store.todos);
      todosSourceRef := Some(todosSource);
      todosSource;
    };

  logTodoIds("[todo] updateTodos before", source.get());
  source.update(current => reducer(current));
  logTodoIds("[todo] updateTodos after", source.get());
};

let addTodo = (store: t, text: string) => {
  let newTodo: todo = {
    id: nextTodoId(store.todos),
    text,
    completed: false,
  };
  log("[todo] addTodo", newTodo);
  updateTodos(~store, todos =>
    StoreCrud.upsert(~getId=(todo: todo) => todo.id, todos, newTodo)
  );
};

let toggleTodo = (store: t, id: string) => {
  log("[todo] toggleTodo", id);
  updateTodos(~store, todos =>
    Js.Array.map(
      ~f=(todo: todo) =>
        todo.id == id ? {...todo, completed: !todo.completed} : todo,
      todos,
    )
  );
};

let removeTodo = (store: t, id: string) => {
  log("[todo] removeTodo", id);
  updateTodos(~store, todos =>
    StoreCrud.remove(~getId=(todo: todo) => todo.id, todos, id)
  );
};
