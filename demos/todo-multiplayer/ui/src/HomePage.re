[@platform js]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@platform native]
module Style = {
  let _css = ();
};

open Tilia.React;
module Hooks = TodoStore.Hooks;
open Hooks;

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
    let _ = useQuery((module RealtimeSchema.Queries.GetList), {list_id: listId}, ());
    let store = useQuery((module RealtimeSchema.Queries.GetListInfo), {id: listId}, ());

    let (newTodoText, setNewTodoText) = React.useState(() => "");

    let addTodo = useMutationFn((module TodoStore.Mutations.AddTodo), ());
    let setTodoCompleted =
      useMutationFn((module TodoStore.Mutations.SetTodoCompleted), ());
    let removeTodo =
      useMutationFn((module TodoStore.Mutations.RemoveTodo), ());
    let failServer =
      useMutationFn((module TodoStore.Mutations.FailServer), ());

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
          addTodo({
            id: UUID.make(),
            list_id: listId,
            text,
          });
        setNewTodoText(_ => "");
      };
    };

    let handleToggleTodo = (id: string, completed: bool) => {
      let _ =
        setTodoCompleted({
          id,
          completed: !completed,
        });
      ();
    };

    let handleRemoveTodo = (id: string) => {
      let _ = removeTodo({id: id});
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
              let _ =
                failServer()
                |> Js.Promise.catch(_error => Js.Promise.resolve());
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
