[@platform js]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@platform native]
module Style = {
  let _css = ();
};

open Model;
open Tilia.React;

let copyUrl = () =>
  switch%platform (Runtime.platform) {
  | Server => ()
  | Client => {
      [@platform js]
      ()
    }
  };

[@platform js]
let handleInputChange = (setNewTodoText, event) => {
  setNewTodoText(_ => React.Event.Form.target(event)##value);
};

[@platform native]
let handleInputChange = (setNewTodoText, _event) => {
  setNewTodoText(_ => "");
};

[@platform js]
let handleFormSubmit = (store, newTodoText, setNewTodoText, event) => {
  React.Event.Synthetic.preventDefault(event);
  let text = String.trim(newTodoText);
  if (String.length(text) > 0) {
    TodoStore.addTodo(store, text);
    setNewTodoText(_ => "");
  };
};

[@platform native]
let handleFormSubmit = (_store, _newTodoText, _setNewTodoText, _event) => ();

let classNameForTodo = (todo: Todo.t) =>
  "todo-item" ++ if (todo.completed) { " completed" } else { "" };

[@react.component]
let make = () => {
  useTilia();
  let store = TodoStore.Context.useStore();
  let (newTodoText, setNewTodoText) = React.useState(() => "");
  let todos = store.config.todos;
  let listName =
    switch (store.config.list) {
    | Some(list) => list.name
    | None => "My Todo List"
    };
  let completedCount =
    todos->Js.Array.filter(~f=(todo: Todo.t) => todo.completed)->Array.length;
  let totalCount = Array.length(todos);

  <div className="todo-container">
    <div className="todo-header">
      <h1> {React.string(listName)} </h1>
      <button className="share-button" onClick={_ => copyUrl()}>
        {React.string("Share")}
      </button>
    </div>
    <form
      className="todo-form"
      onSubmit=(event =>
        handleFormSubmit(store, newTodoText, setNewTodoText, event)
      )>
      <input
        type_="text"
        value=newTodoText
        placeholder="What needs to be done?"
        onChange=(event => handleInputChange(setNewTodoText, event))
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
           todos->Js.Array.map(~f=(todo: Todo.t) =>
             <div key=todo.id className=classNameForTodo(todo)>
                <input
                  type_="checkbox"
                  checked=todo.completed
                  onChange={_ => TodoStore.toggleTodo(store, todo.id)}
                 />
                <span className="todo-text">
                  {React.string(todo.text)}
                </span>
                <button
                  className="todo-delete"
                  onClick={_ => TodoStore.removeTodo(store, todo.id)}>
                  <Lucide.IconX />
                </button>
             </div>
           )
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
  </div>;
};
