open Melange_json.Primitives;

[@deriving json]
type state = Model.t;

type subscription = RealtimeSubscription.t;

type add_todo_payload = {
  id: string,
  list_id: string,
  text: string,
};

type set_todo_completed_payload = {
  id: string,
  completed: bool,
};

type action =
  | AddTodo(add_todo_payload)
  | SetTodoCompleted(set_todo_completed_payload)
  | RemoveTodo(string)
  | FailServerMutation
  | FailClientMutation;

type store = {
  list_id: string,
  state: state,
  completed_count: int,
  total_count: int,
};

let emptyState: state = {
  todos: [||],
  list: None,
};

let scopeKeyOfState = (state: state) =>
  switch (state.list) {
  | Some(list) => list.id
  | None => "default"
  };

let timestampOfState = (state: state) =>
  switch (state.list) {
  | Some(list) => list.updated_at
  | None => 0.0
  };

let setTimestamp = (~state: state, ~timestamp: float) =>
  switch (state.list) {
  | Some(list) => {
      ...state,
      list: Some({...list, updated_at: timestamp}),
    }
  | None => state
  };

let action_to_json = action =>
  switch (action) {
  | AddTodo(payload) =>
    StoreJson.parse(
      "{\"kind\":\"add_todo\",\"payload\":{\"id\":"
      ++ string_to_json(payload.id)->Melange_json.to_string
      ++ ",\"list_id\":"
      ++ string_to_json(payload.list_id)->Melange_json.to_string
      ++ ",\"text\":"
      ++ string_to_json(payload.text)->Melange_json.to_string
      ++ "}}",
    )
  | SetTodoCompleted(payload) =>
    StoreJson.parse(
      "{\"kind\":\"set_todo_completed\",\"payload\":{\"id\":"
      ++ string_to_json(payload.id)->Melange_json.to_string
      ++ ",\"completed\":"
      ++ bool_to_json(payload.completed)->Melange_json.to_string
      ++ "}}",
    )
  | RemoveTodo(id) =>
    StoreJson.parse(
      "{\"kind\":\"remove_todo\",\"payload\":{\"id\":"
      ++ string_to_json(id)->Melange_json.to_string
      ++ "}}",
    )
  | FailServerMutation =>
    StoreJson.parse("{\"kind\":\"fail_server_mutation\",\"payload\":{}}")
  | FailClientMutation =>
    StoreJson.parse("{\"kind\":\"fail_client_mutation\",\"payload\":{}}")
  };

let action_of_json = json => {
  let kind =
    StoreJson.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
  let payload =
    StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
  switch (kind) {
  | "add_todo" =>
    AddTodo({
      id: StoreJson.requiredField(~json=payload, ~fieldName="id", ~decode=string_of_json),
      list_id:
        StoreJson.requiredField(~json=payload, ~fieldName="list_id", ~decode=string_of_json),
      text: StoreJson.requiredField(~json=payload, ~fieldName="text", ~decode=string_of_json),
    })
  | "set_todo_completed" =>
    SetTodoCompleted({
      id: StoreJson.requiredField(~json=payload, ~fieldName="id", ~decode=string_of_json),
      completed:
        StoreJson.requiredField(~json=payload, ~fieldName="completed", ~decode=bool_of_json),
    })
  | "remove_todo" =>
    RemoveTodo(
      StoreJson.requiredField(~json=payload, ~fieldName="id", ~decode=string_of_json),
    )
  | "fail_server_mutation" => FailServerMutation
  | _ => FailClientMutation
  };
};

let reduce = (~state: state, ~action: action) => {
  let updatedAt = Js.Date.now();
  let withTimestamp = nextState => setTimestamp(~state=nextState, ~timestamp=updatedAt);
  switch (action) {
  | AddTodo(payload) =>
    withTimestamp({
      ...state,
      todos:
        StoreCrud.upsert(
          ~getId=(item: Model.Todo.t) => item.id,
          state.todos,
          {
            id: payload.id,
            list_id: payload.list_id,
            text: payload.text,
            completed: false,
          },
        ),
    })
  | SetTodoCompleted(payload) =>
    withTimestamp({
      ...state,
      todos:
        Js.Array.map(
          ~f=(item: Model.Todo.t) =>
            item.id == payload.id ? {...item, completed: payload.completed} : item,
          state.todos,
        ),
    })
  | RemoveTodo(id) =>
    withTimestamp({
      ...state,
      todos: StoreCrud.remove(~getId=(item: Model.Todo.t) => item.id, state.todos, id),
    })
  | FailServerMutation =>
    switch (state.list) {
    | Some(list) =>
      withTimestamp({
        ...state,
        todos:
          StoreCrud.upsert(
            ~getId=(item: Model.Todo.t) => item.id,
            state.todos,
            {id: "fail-server-test", list_id: list.id, text: "Server failure test todo", completed: false},
          ),
      })
    | None => state
    }
  | FailClientMutation => state
  };
};

[@platform js]
let onActionError = message => Sonner.showError(message);

[@platform native]
let onActionError = _message => ();

/* ============================================================================
   New Grouped API (Task 4/6) - Using Synced.DefineCrud
   ============================================================================ */

module StoreDef =
  StoreBuilder.Synced.DefineCrud({
    type nonrec state = state;
    type nonrec action = action;
    type nonrec store = store;
    type nonrec subscription = subscription;
    type row = Model.Todo.t;

    let input =
      StoreBuilder.make()
      |> StoreBuilder.withSchema({
           emptyState,
           reduce,
           makeStore:
             (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
             {
               list_id:
                 switch (state.list) {
                 | Some(list) => list.id
                 | None => ""
                 },
               state,
               completed_count:
                 StoreBuilder.Synced.Crud.filteredCount(
                   ~derive?,
                   ~getItems=(store: store) => store.state.todos,
                   ~predicate=(item: Model.Todo.t) => item.completed,
                   (),
                 ),
               total_count:
                 StoreBuilder.Synced.Crud.totalCount(
                   ~derive?,
                   ~getItems=(store: store) => store.state.todos,
                   (),
                 ),
             };
           },
         })
      |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
      |> StoreBuilder.withSyncCrud(
           ~storeName = "todo-multiplayer",
           ~scopeKeyOfState,
           ~timestampOfState,
           ~setTimestamp,
           ~transport = {
             subscriptionOfState: (state: state): option(subscription) =>
               switch (state.list) {
               | Some(list) => Some(RealtimeSubscription.list(list.id))
               | None => None
               },
             encodeSubscription: RealtimeSubscription.encode,
             eventUrl: Constants.event_url,
             baseUrl: Constants.base_url,
           },
           ~table=RealtimeSchema.table_name("todos"),
           ~decodeRow=Model.Todo.of_json,
           ~getId=(todo: Model.Todo.t) => todo.id,
           ~getItems=(state: state) => state.todos,
           ~setItems=(state: state, items) => {...state, todos: items},
           ~hooks={
             StoreBuilder.Sync.onActionError: Some(onActionError),
             onActionAck: None,
             onCustom: None,
             onMedia: None,
             onError: None,
             onOpen: None,
             onConnectionHandle: None,
           },
           ~stateElementId=Some("initial-store"),
           (),
         );
  });

/* Re-export with the same interface as before */
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
  let list_id =
    switch (store.state.list) {
    | Some(list) => list.id
    | None => store.list_id
    };
  dispatch(AddTodo({
    id: UUID.make(),
    list_id,
    text,
  }));
};

let toggleTodo = (store: t, id: string) =>
  switch (Js.Array.find(~f=(todo: Model.Todo.t) => todo.id == id, store.state.todos)) {
  | Some(todo) => dispatch(SetTodoCompleted({id, completed: !todo.completed}))
  | None => ()
  };

let removeTodo = (_store: t, id: string) => dispatch(RemoveTodo(id));

let failServerMutation = (_store: t) => dispatch(FailServerMutation);

let failClientMutation = (_store: t) => dispatch(FailClientMutation);
