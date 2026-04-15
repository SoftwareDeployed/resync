[@platform js]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@platform native]
module Style = {
  let _css = ();
};

open Tilia.React;

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
let make = (~listId: string) => {
  useTilia();
  let (newTodoText, setNewTodoText) = React.useState(() => "");

  // Query for todos
  let todosQuery =
    UseQuery.useQuery(
      (module RealtimeSchema.Queries.GetList),
      { list_id: listId },
      (),
    );
  // Query for list info
  let listInfoQuery =
    UseQuery.useQuery(
      (module RealtimeSchema.Queries.GetListInfo),
      { id: listId },
      (),
    );

  // Mutations
  let addTodoMutation =
    UseMutation.make((module RealtimeSchema.Mutations.AddTodo), ());
  let setTodoCompletedMutation =
    UseMutation.make((module RealtimeSchema.Mutations.SetTodoCompleted), ());
  let removeTodoMutation =
    UseMutation.make((module RealtimeSchema.Mutations.RemoveTodo), ());

  // Extract data from queries
  let todos =
    switch (todosQuery.data) {
    | Loaded(rows) => rows
    | _ => [||]
    };

  let listName =
    switch (listInfoQuery.data) {
    | Loaded([|list|]) => list.name
    | _ => "My Todo List"
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
    let _ = removeTodoMutation.dispatch({ id: id });
    ();
  };

  // Loading and error states
  let isLoading = todosQuery.loading || listInfoQuery.loading;
  let errorMessage =
    switch (todosQuery.error, listInfoQuery.error) {
    | (Some(msg), _) => Some(msg)
    | (_, Some(msg)) => Some(msg)
    | _ => None
    };

  <div className="todo-container">
    <div className="todo-header">
      <h1> {React.string(listName)} </h1>
      <div>
        <button
          className="share-button" type_="button" onClick={_ => copyUrl()}>
          {React.string("Share")}
        </button>
      </div>
    </div>
    {switch (isLoading, errorMessage) {
     | (true, _) =>
       <div className="todo-empty"> {React.string("Loading...")} </div>
     | (_, Some(msg)) =>
       <div className="todo-empty"> {React.string("Error: " ++ msg)} </div>
     | _ =>
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
                      onChange={_ =>
                        handleToggleTodo(todo.id, todo.completed)
                      }
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
     }}
  </div>;
};
