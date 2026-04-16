//open Melange_json.Primitives;

module Todo = {
  //[@deriving json]
  type t = RealtimeSchema.todos;
};

module TodoList = {
  //[@deriving json]
  type t = RealtimeSchema.todo_lists;
};

//[@deriving json]
type t = {
  todos: array(Todo.t),
  list: option(TodoList.t),
};
