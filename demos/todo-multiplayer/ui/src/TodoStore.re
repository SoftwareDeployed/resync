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
let emptyPayload = () => Melange_json.declassify(`Assoc([]));
let customAction_to_json = (~action) => {
  let dict = Js.Dict.empty();
  Js.Dict.set(dict, "kind", string_to_json(switch (action) { | FailServerMutation => "fail_server_mutation" | FailClientMutation => "fail_client_mutation" }));
  Js.Dict.set(dict, "payload", emptyPayload());
  Melange_json.declassify(`Assoc(Array.to_list(Js.Dict.entries(dict))));
};
let state_of_json = Model.of_json;
let state_to_json = Model.to_json;
let customAction_of_json = (~kind, ~payload as _payload) => switch (kind) {
  | "fail_server_mutation" => FailServerMutation
  | _ => FailClientMutation
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
  |> StoreBuilder.withJson(
       ~state_of_json,
       ~state_to_json,
       ~action_of_json=RealtimeSchema.MutationActions.action_of_json(~custom_of_json=customAction_of_json),
       ~action_to_json=RealtimeSchema.MutationActions.action_to_json(~custom_to_json=customAction_to_json),
     )
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
