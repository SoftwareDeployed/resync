open Melange_json.Primitives;

module Todo = {
  [@deriving json]
  type t = {
    id: string,
    list_id: string,
    text: string,
    completed: bool,
    created_at: float,
  };
};

module TodoList = {
  [@deriving json]
  type t = {
    id: string,
    name: string,
    updated_at: float,
  };
};

[@deriving json]
type t = {
  todos: array(Todo.t),
  list: option(TodoList.t),
};
