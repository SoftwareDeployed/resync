[@platform js]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@platform native]
module Style = {
  let _css = ();
};

open Tilia.React;
open Hooks;

module RawTodoStore = TodoStore;
module TodoStore = (
  RawTodoStore:
    StoreBuilder.Runtime.Exports
      with type state = RawTodoStore.state
      and type action = RawTodoStore.action
      and type t = RawTodoStore.t
);

module FailServerMutationDef = {
  type params = unit;
  let name = "fail_server_mutation";
  let encodeParams = (_: unit) => Melange_json.declassify(`Assoc([]));
};

let copyUrl = () =>
  switch%platform (Runtime.platform) {
  | Server => ()
  | Client => [@platform js] ignore(Sonner.copyCurrentUrl())
  };

[@platform js]
let handleInputChange = (setNewTodoText, event) => {
  setNewTodoText(_ => React.Event.Form.target(event)##value);
};

[@platform native]
let handleInputChange = (setNewTodoText, _event) => {
  setNewTodoText(_ => "");
};

[@react.component]
let make =
  leaf((~listId: string) => {
    let _ = TodoStore.useQuery((module RealtimeSchema.Queries.GetList), {list_id: listId}, ());
    let store = TodoStore.useQuery((module RealtimeSchema.Queries.GetListInfo), {id: listId}, ());

    let (newTodoText, setNewTodoText) = React.useState(() => "");

    // Mutations - dispatch through the store
    let addTodoMutation =
      useMutation(
        (module RealtimeSchema.Mutations.AddTodo),
        ~onDispatch=
          params =>
            TodoStore.dispatch(
              RealtimeSchema.MutationActions.AddTodo({
                id: params.id,
                list_id: params.list_id,
                text: params.text,
              }),
            ),
        (),
      );
    let setTodoCompletedMutation =
      useMutation(
        (module RealtimeSchema.Mutations.SetTodoCompleted),
        ~onDispatch=
          params =>
            TodoStore.dispatch(
              RealtimeSchema.MutationActions.SetTodoCompleted({
                id: params.id,
                completed: params.completed,
              }),
            ),
        (),
      );
    let removeTodoMutation =
      useMutation(
        (module RealtimeSchema.Mutations.RemoveTodo),
        ~onDispatch=
          params =>
            TodoStore.dispatch(
              RealtimeSchema.MutationActions.RemoveTodo(
                params.id,
              ),
            ),
        (),
      );
    let failServerMutation =
      useMutation(
        (module FailServerMutationDef),
        ~onDispatch=
          _ =>
            TodoStore.dispatch(
              RealtimeSchema.MutationActions.Custom(
                RawTodoStore.FailServerMutation,
              ),
            ),
        (),
      );

    let todos = store.state.todos;

    let listName =
      switch (store.state.list) {
      | Some(list) => list.name
      | None => "My Todo List"
      };

    let completedCount =
      todos->Js.Array.filter(~f=todo => todo.completed)->Array.length;
    let totalCount = Array.length(todos);

    // Handlers
    let handleAddTodo = () => {
      let text = String.trim(newTodoText);
      if (String.length(text) > 0) {
        let _ =
          addTodoMutation.dispatch({
            id: UUID.make(),
            list_id: listId,
            text,
          });
        setNewTodoText(_ => "");
      };
    };

    let handleToggleTodo = (id: string, completed: bool) => {
      let _ =
        setTodoCompletedMutation.dispatch({
          id,
          completed: !completed,
        });
      ();
    };

    let handleRemoveTodo = (id: string) => {
      Js.Console.log(id);
      let _ = removeTodoMutation.dispatch({ id: id });
      ();
    };

    <div className="todo-container">
      <div className="todo-header">
        <h1> {React.string(listName)} </h1>
        <div>
          <button
            className="share-button" type_="button" onClick={_ => copyUrl()}>
            {React.string("Share")}
          </button>
          <button
            className="share-button"
            type_="button"
            onClick={_ => {
              let _ = failServerMutation.dispatch();
              ();
            }}>
            {React.string("Fail Query")}
          </button>
        </div>
      </div>
      <>
        <form
          className="todo-form"
          onSubmit={event => {
            React.Event.Synthetic.preventDefault(event);
            handleAddTodo();
          }}>
          <input
            type_="text"
            value=newTodoText
            placeholder="What needs to be done?"
            onChange={event => handleInputChange(setNewTodoText, event)}
          />
          <button type_="submit"> {React.string("Add")} </button>
        </form>
        <div className="todo-list">
          {switch (Array.length(todos)) {
           | 0 =>
             <div className="todo-empty">
               {React.string("No todos yet. Add one above!")}
             </div>
           | _ =>
             React.array(
               todos->Js.Array.map(~f=todo =>
                 <div
                   key={todo.id}
                   className={
                     "todo-item" ++ (todo.completed ? " completed" : "")
                   }>
                   <input
                     type_="checkbox"
                     checked={todo.completed}
                     onChange={_ => handleToggleTodo(todo.id, todo.completed)}
                   />
                   <span className="todo-text">
                     {React.string(todo.text)}
                   </span>
                   <button
                     className="todo-delete"
                     onClick={_ => handleRemoveTodo(todo.id)}>
                     <Lucide.IconX />
                   </button>
                 </div>
               ),
             )
           }}
        </div>
        <div className="todo-stats">
          {React.string(
             string_of_int(completedCount)
             ++ " of "
             ++ string_of_int(totalCount)
             ++ " completed",
           )}
        </div>
      </>
    </div>;
  });
