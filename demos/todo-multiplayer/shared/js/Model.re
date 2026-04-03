open Melange_json.Primitives;

module Todo = {
  [@deriving json]
  type t = {
    id: string,
    list_id: string,
    text: string,
    completed: bool,
  };
};

module TodoList = {
  [@deriving json]
  type t = {
    id: string,
    name: string,
  };
};

[@deriving json]
type t = {
  todos: array(Todo.t),
  list: option(TodoList.t),
};
