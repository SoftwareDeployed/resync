[@platform js]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@platform native]
module Style = {
  let _css = ();
};

open Tilia.React;

[@platform js]
let getInputValue = event => React.Event.Form.target(event)##value;

[@platform native]
let getInputValue = _event => "";

[@platform js]
let preventDefault = event => React.Event.Synthetic.preventDefault(event);

[@platform native]
let preventDefault = _event => ();

[@react.component]
let make =
  leaf(() => {
    useTilia();
    module Hooks = TodoStore.Hooks;
    open Hooks;
    let store = TodoStore.Context.useStore();
    let (newTodoText, setNewTodoText) = React.useState(() => "");
    let addTodoMutation = useMutation((module TodoStore.Mutations.AddTodo), ());
    let setTodoCompletedMutation =
      useMutation((module TodoStore.Mutations.SetTodoCompleted), ());
    let removeTodoMutation = useMutation((module TodoStore.Mutations.RemoveTodo), ());

    let completed_count =
      store.state.todos
      ->Js.Array.filter(~f=(todo: TodoStore.todo) => todo.completed)
      ->Array.length;

    let total_count = Array.length(store.state.todos);

    let handleSubmit = event => {
      preventDefault(event);
      let text = String.trim(newTodoText);
      if (text != "") {
        let todo: TodoStore.todo = {
          id: TodoStore.nextTodoId(store.state.todos),
          text,
          completed: false,
        };
        let _ = addTodoMutation.mutate(todo);
        setNewTodoText(_ => "");
      };
    };

    let handleInputChange = event => {
      let value = getInputValue(event);
      setNewTodoText(_ => value);
    };

    let handleToggleTodo = id =>
      switch (Js.Array.find(~f=(item: TodoStore.todo) => item.id == id, store.state.todos)) {
      | Some(todo) =>
        let input: TodoStore.set_completed = {id, completed: !todo.completed};
        let _ = setTodoCompletedMutation.mutate(input);
        ()
      | None => ()
      };

    let handleRemoveTodo = id => {
      let _ = removeTodoMutation.mutate(id);
      ();
    };

    <div className="todo-container">
      <div className="todo-header">
        <h1> {React.string("My Todo List")} </h1>
      </div>
      <form className="todo-form" onSubmit=handleSubmit>
        <input
          className="todo-input"
          type_="text"
          value=newTodoText
          onChange=handleInputChange
          placeholder="Add a new todo..."
        />
        <button className="todo-button" type_="submit">
          {React.string("Add")}
        </button>
      </form>
        <div className="todo-list">
           {if (total_count == 0) {
             <div className="todo-empty">
               {React.string("No todos yet. Add one above!")}
             </div>;
            } else {
             store.state.todos->Js.Array.map(~f=(todo: TodoStore.todo) => {
                 let textClassName =
                   "todo-text " ++ (todo.completed ? "completed" : "");

                 <div key={todo.id} className="todo-item">
                   <input
                      className="todo-checkbox"
                      type_="checkbox"
                      checked={todo.completed}
                      onChange={_ => handleToggleTodo(todo.id)}
                    />
                   <span className=textClassName>
                     {React.string(todo.text)}
                   </span>
                      <button
                        className="todo-delete"
                       onClick={_ => handleRemoveTodo(todo.id)}>
                       <Lucide.IconX size=20 />
                      </button>
                  </div>;
                })
             |> React.array;
          }}
        </div>
       {if (total_count > 0) {
          <div className="todo-stats">
            {React.string(
               string_of_int(completed_count)
               ++ " of "
               ++ string_of_int(total_count)
               ++ " completed",
             )}
          </div>;
        } else {
         React.null;
       }}
    </div>;
  });
