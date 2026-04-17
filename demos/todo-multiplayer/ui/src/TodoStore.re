open Melange_json.Primitives;
type customAction = | FailServerMutation | FailClientMutation;
type action = RealtimeSchema.MutationActions.action(customAction);
type state = Model.t;
type subscription = RealtimeSubscription.t;
type store = { list_id: string, state, completed_count: int, total_count: int };
let emptyState: state = { todos: [||], list: None };
let scopeKeyOfState = (state: state) => switch (state.list) { | Some(list) => list.id | None => "default" };
let timestampOfState = (state: state) => switch (state.list) { | Some(list) => list.updated_at | None => 0.0 };
let setTimestamp = (~state: state, ~timestamp: float) => switch (state.list) {
  | Some(list) => {...state, list: Some({...list, updated_at: timestamp})} | None => state };
let action_to_json = action => switch (action) {
  | RealtimeSchema.MutationActions.AddTodo(p) => StoreJson.parse(
      "{\"kind\":\"add_todo\",\"payload\":{\"id\":\"" ++ p.id ++ "\",\"list_id\":\"" ++ p.list_id ++ "\",\"text\":\"" ++ p.text ++ "\"}}")
  | RealtimeSchema.MutationActions.SetTodoCompleted(p) => StoreJson.parse(
      "{\"kind\":\"set_todo_completed\",\"payload\":{\"id\":\"" ++ p.id ++ "\",\"completed\":" ++ (bool_to_json(p.completed)->Melange_json.to_string) ++ "}}")
  | RealtimeSchema.MutationActions.RemoveTodo(id) => StoreJson.parse(
      "{\"kind\":\"remove_todo\",\"payload\":{\"id\":\"" ++ id ++ "\"}}")
  | RealtimeSchema.MutationActions.CreateList(id) => StoreJson.parse(
      "{\"kind\":\"create_list\",\"payload\":{\"id\":\"" ++ id ++ "\"}}")
  | RealtimeSchema.MutationActions.RenameList(p) => StoreJson.parse(
      "{\"kind\":\"rename_list\",\"payload\":{\"id\":\"" ++ p.id ++ "\",\"name\":\"" ++ p.name ++ "\"}}")
  | RealtimeSchema.MutationActions.Custom(FailServerMutation) => StoreJson.parse(
      "{\"kind\":\"fail_server_mutation\",\"payload\":{}}")
  | RealtimeSchema.MutationActions.Custom(FailClientMutation) => StoreJson.parse(
      "{\"kind\":\"fail_client_mutation\",\"payload\":{}}")
  };
let req = (j, n, d) => StoreJson.requiredField(~json=j, ~fieldName=n, ~decode=d);
let state_of_json = Model.of_json;
let state_to_json = Model.to_json;
let action_of_json = json => {
  let kind = req(json, "kind", string_of_json);
  let p = req(json, "payload", v => v);
  switch (kind) {
  | "add_todo" => RealtimeSchema.MutationActions.AddTodo({id: req(p, "id", string_of_json), list_id: req(p, "list_id", string_of_json), text: req(p, "text", string_of_json)})
  | "set_todo_completed" => RealtimeSchema.MutationActions.SetTodoCompleted({id: req(p, "id", string_of_json), completed: req(p, "completed", bool_of_json)})
  | "remove_todo" => RealtimeSchema.MutationActions.RemoveTodo(req(p, "id", string_of_json))
  | "create_list" => RealtimeSchema.MutationActions.CreateList(req(p, "id", string_of_json))
  | "rename_list" => RealtimeSchema.MutationActions.RenameList({id: req(p, "id", string_of_json), name: req(p, "name", string_of_json)})
  | "fail_server_mutation" => RealtimeSchema.MutationActions.Custom(FailServerMutation)
  | _ => RealtimeSchema.MutationActions.Custom(FailClientMutation)
  };
};
let upsert = (todos, item) => StoreCrud.upsert(~getId=(i: Model.Todo.t) => i.id, todos, item);
let reduce = (~state: state, ~action: action) => {
  let ts = Js.Date.now();
  let wt = s => setTimestamp(~state=s, ~timestamp=ts);
  switch (action) {
  | RealtimeSchema.MutationActions.AddTodo(p) =>
    wt({...state, todos: upsert(state.todos, {id: p.id, list_id: p.list_id, text: p.text, completed: false, created_at: Js.Date.now()})})
  | RealtimeSchema.MutationActions.SetTodoCompleted(p) =>
    wt({...state, todos: Js.Array.map(~f=(item: Model.Todo.t) => item.id == p.id ? {...item, completed: p.completed} : item, state.todos)})
  | RealtimeSchema.MutationActions.RemoveTodo(id) =>
    wt({...state, todos: StoreCrud.remove(~getId=(i: Model.Todo.t) => i.id, state.todos, id)})
  | RealtimeSchema.MutationActions.CreateList(id) =>
    wt({...state, list: Some({id, name: "My Todo List", updated_at: ts})})
  | RealtimeSchema.MutationActions.RenameList(p) =>
    (switch (state.list) { | Some(_l) => wt({...state, list: Some({id: p.id, name: p.name, updated_at: ts})}) | None => state })
  | RealtimeSchema.MutationActions.Custom(FailServerMutation) =>
    (switch (state.list) {
     | Some(l) => wt({...state, todos: upsert(state.todos, {id: "fail-server-test", list_id: l.id, text: "Server failure test todo", completed: false, created_at: Js.Date.now()})})
     | None => state })
  | RealtimeSchema.MutationActions.Custom(FailClientMutation) => state
  };
};
[@platform js]
let onActionError = message => Sonner.showError(message);
[@platform native]
let onActionError = _message => ();
module StoreDef = (val StoreBuilder.buildCrud(StoreBuilder.make()
  |> StoreBuilder.withSchema({ emptyState, reduce,
       makeStore: (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
         {
           list_id: switch (state.list) { | Some(list) => list.id | None => "" },
           state,
           completed_count: StoreBuilder.Crud.filteredCount(~derive?, ~getItems=(s: store) => s.state.todos, ~predicate=(item: Model.Todo.t) => item.completed, ()),
           total_count: StoreBuilder.Crud.totalCount(~derive?, ~getItems=(s: store) => s.state.todos, ()),
         };
       } })
  |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
  |> StoreBuilder.withSyncCrud(~storeName="todo-multiplayer", ~scopeKeyOfState, ~timestampOfState, ~setTimestamp,
       ~transport={
         subscriptionOfState: (state: state) => (switch (state.list) {
           | Some(list) => Some(RealtimeSubscription.list(list.id)) | None => None }: option(subscription)),
         encodeSubscription: RealtimeSubscription.encode,
         eventUrl: Constants.event_url, baseUrl: Constants.base_url },
       ~table=RealtimeSchema.table_name("todos"), ~decodeRow=Model.Todo.of_json,
       ~getId=(todo: Model.Todo.t) => todo.id, ~getItems=(state: state) => state.todos,
       ~setItems=(state: state, items) => {...state, todos: items},
       ~hooks={ StoreBuilder.Sync.onActionError: Some(onActionError),
         onActionAck: None, onCustom: None, onMedia: None, onError: None, onOpen: None, onMultiplexedHandle: None },
       ~stateElementId=Some("initial-store"), ())
));
include (StoreDef: StoreBuilder.Runtime.Exports with type state := state and type action := action and type t := store);
type t = store;
module Context = StoreDef.Context;
let addTodo = (store: t, text: string) => {
  let list_id = switch (store.state.list) { | Some(list) => list.id | None => store.list_id };
  dispatch(RealtimeSchema.MutationActions.AddTodo({id: UUID.make(), list_id, text}));
};
let toggleTodo = (store: t, id: string) =>
  switch (Js.Array.find(~f=(todo: Model.Todo.t) => todo.id == id, store.state.todos)) {
  | Some(todo) => dispatch(RealtimeSchema.MutationActions.SetTodoCompleted({id, completed: !todo.completed}))
  | None => ()
  };
let removeTodo = (_store: t, id: string) =>
  dispatch(RealtimeSchema.MutationActions.RemoveTodo(id));
let failServerMutation = (_store: t) =>
  dispatch(RealtimeSchema.MutationActions.Custom(FailServerMutation));
let failClientMutation = (_store: t) =>
  dispatch(RealtimeSchema.MutationActions.Custom(FailClientMutation));
