open Melange_json.Primitives;

[@deriving json]
type todo = {
  id: string,
  text: string,
  completed: bool,
};

[@deriving json]
type config = {
  todos: list(todo),
};
