open Melange_json.Primitives;

[@deriving json]
type state = Model.t;

type subscription = RealtimeSubscription.t;

type patch = StoreCrud.patch(Model.Todo.t);

[@deriving json]
type payload = state;

type store = {
  list_id: string,
  state: state,
  completed_count: int,
  total_count: int,
};

[@deriving json]
type add_todo_input = {
  id: string,
  list_id: string,
  text: string,
};

[@deriving json]
type todo_id_input = {id: string};

let emptyState: state = {
  todos: [||],
  list: None,
};

let emptyPayload: payload = emptyState;

let storageKeyOfState = (state: state) =>
  switch (state.list) {
  | Some(list) => "todo-multiplayer." ++ list.id
  | None => "todo-multiplayer"
  };

let randomHex = length => {
  let chars = "0123456789abcdef";
  let buffer = Buffer.create(length);
  for (i in 1 to length) {
    let _ = i;
    Buffer.add_char(buffer, chars.[Random.int(16)]);
  };
  Buffer.contents(buffer);
};

let randomUuid = () => {
  Random.self_init();
  Printf.sprintf(
    "%s-%s-%s-%s-%s",
    randomHex(8),
    randomHex(4),
    randomHex(4),
    randomHex(4),
    randomHex(12),
  );
};

module Local = StoreLocal.Make({
  type nonrec state = state;
  type nonrec payload = payload;

  module Adapter = StoreLocal.LocalStorageAdapter;

  let storageKeyOfState = storageKeyOfState;
  let payloadOfState = (state: state): payload => state;
  let stateOfPayload = (payload: payload): state => payload;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;
});

module Remote = StoreSync.Make({
  type nonrec state = state;
  type nonrec patch = patch;
  type nonrec subscription = subscription;

  let subscriptionOfState = (state: state): option(subscription) =>
    switch (state.list) {
    | Some(list) => Some(RealtimeSubscription.list(list.id))
    | None => None
    };
  let encodeSubscription = RealtimeSubscription.encode;
  let updatedAtOf = (_state: state) => 0.0;
  let state_of_json = state_of_json;

  let decodePatch =
    StorePatch.compose([
      StoreCrud.decodePatch(
        ~table=RealtimeSchema.table_name("todos"),
        ~decodeRow=Model.Todo.of_json,
        (),
      ),
    ]);

  let updateOfPatch = StoreCrud.updateOfPatch(
    ~getId=(todo: Model.Todo.t) => todo.id,
    ~getItems=(state: state) => state.todos,
    ~setItems=(state: state, items) => {...state, todos: items},
  );

  let eventUrl = Constants.event_url;
  let baseUrl = Constants.base_url;
});

module Layered = StoreBuilder.Layered.Make({
  type nonrec state = state;
  type nonrec payload = payload;
  type nonrec store = store;

  let emptyStore: store = {
    list_id: "",
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
  let clientLayers = [|Local.hooks, Remote.hooks|];

  let makeStore =
      (
        ~state: state,
        ~payload: payload,
        ~derive: option(Tilia.Core.deriver(store))=?,
        (),
      ):
      store => {
    let _ = payload;
    let todos = state.todos;
    let completedCount = (todos: array(Model.Todo.t)) =>
      todos
      ->Js.Array.filter(~f=(todo: Model.Todo.t) => todo.completed)
      ->Array.length;
    {
      list_id:
        switch (state.list) {
        | Some(list) => list.id
        | None => ""
        },
      state,
      completed_count:
        StoreBuilder.derived(
          ~derive?,
          ~client=(store: store) => completedCount(store.state.todos),
          ~server=() => completedCount(todos),
          (),
        ),
      total_count:
        StoreBuilder.derived(
          ~derive?,
          ~client=(store: store) => Array.length(store.state.todos),
          ~server=() => Array.length(todos),
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

let optimisticUpsert = (todo: Model.Todo.t) =>
  Local.update(state => {
    let todos =
      StoreCrud.upsert(~getId=(item: Model.Todo.t) => item.id, state.todos, todo);
    {...state, todos};
  });

let optimisticRemove = (id: string) =>
  Local.update(state => {
    let todos = StoreCrud.remove(~getId=(todo: Model.Todo.t) => todo.id, state.todos, id);
    {...state, todos};
  });

let addTodo = (store: t, text: string) => {
  let list_id =
    switch (store.state.list) {
    | Some(list) => list.id
    | None => store.list_id
    };
  let id = randomUuid();
  optimisticUpsert({
    id,
    list_id,
    text,
    completed: false,
  });
  RealtimeClient.Socket.sendMutation(
    "add_todo",
    StoreJson.stringify(add_todo_input_to_json, {id, list_id, text}),
  );
};

let toggleTodo = (store: t, id: string) => {
  switch (Js.Array.find(~f=(todo: Model.Todo.t) => todo.id == id, store.state.todos)) {
  | Some(todo) => optimisticUpsert({...todo, completed: !todo.completed})
  | None => ()
  };
  RealtimeClient.Socket.sendMutation(
    "toggle_todo",
    StoreJson.stringify(todo_id_input_to_json, {id: id}),
  );
};

let removeTodo = (_store: t, id: string) => {
  optimisticRemove(id);
  RealtimeClient.Socket.sendMutation(
    "remove_todo",
    StoreJson.stringify(todo_id_input_to_json, {id: id}),
  );
};
