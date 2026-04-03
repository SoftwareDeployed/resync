open Melange_json.Primitives;

[@deriving json]
type config = Model.t;

type subscription = RealtimeSubscription.t;

type patch = StoreCrud.patch(Model.Todo.t);

[@deriving json]
type payload = config;

type store = {
  list_id: string,
  config: config,
  completed_count: int,
  total_count: int,
};

[@deriving json]
type add_todo_input = {
  list_id: string,
  text: string,
};

[@deriving json]
type todo_id_input = {id: string};

module Runtime = StoreBuilder.Runtime.Make({
  type nonrec config = config;
  type nonrec patch = patch;
  type nonrec payload = payload;
  type nonrec store = store;
  type nonrec subscription = subscription;

  let emptyStore: store = {
    list_id: "",
    config: {
      todos: [||],
      list: None,
    },
    completed_count: 0,
    total_count: 0,
  };
  let stateElementId = "initial-store";

  let payloadOfConfig = (config: config): payload => config;
  let configOfPayload = (payload: payload): config => payload;

  let makeStore =
      (
        ~config: config,
        ~payload: payload,
        ~derive: option(Tilia.Core.deriver(store))=?,
        (),
      ):
      store => {
    let _ = payload;
    let todos = config.todos;
    let completedCount = (todos: array(Model.Todo.t)) =>
      todos
      ->Js.Array.filter(~f=(todo: Model.Todo.t) => todo.completed)
      ->Array.length;
    {
      list_id:
        switch (config.list) {
        | Some(list) => list.id
        | None => ""
        },
      config,
      completed_count:
        StoreBuilder.derived(
          ~derive?,
          ~client=(store: store) => completedCount(store.config.todos),
          ~server=() => completedCount(todos),
          (),
        ),
      total_count:
        StoreBuilder.derived(
          ~derive?,
          ~client=(store: store) => Array.length(store.config.todos),
          ~server=() => Array.length(todos),
          (),
        ),
    };
  };

  let config_of_json = config_of_json;
  let config_to_json = config_to_json;
  let payload_of_json = payload_of_json;
  let payload_to_json = payload_to_json;

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
    ~getItems=(config: config) => config.todos,
    ~setItems=(config: config, items) => {...config, todos: items},
  );

  let subscriptionOfConfig = (config: config): option(subscription) =>
    switch (config.list) {
    | Some(list) => Some(RealtimeSubscription.list(list.id))
    | None => None
    };
  let encodeSubscription = RealtimeSubscription.encode;

  let updatedAtOf = (_config: config) => 0.0;
  let eventUrl = Constants.event_url;
  let baseUrl = Constants.base_url;
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

let addTodo = (store: t, text: string) => {
  let list_id =
    switch (store.config.list) {
    | Some(list) => list.id
    | None => store.list_id
    };
  RealtimeClient.Socket.sendMutation(
    "add_todo",
    StoreJson.stringify(
      add_todo_input_to_json,
      {
        list_id,
        text,
      },
    ),
  );
};

let toggleTodo = (_store: t, id: string) =>
  RealtimeClient.Socket.sendMutation(
    "toggle_todo",
    StoreJson.stringify(todo_id_input_to_json, {id: id}),
  );

let removeTodo = (_store: t, id: string) =>
  RealtimeClient.Socket.sendMutation(
    "remove_todo",
    StoreJson.stringify(todo_id_input_to_json, {id: id}),
  );
