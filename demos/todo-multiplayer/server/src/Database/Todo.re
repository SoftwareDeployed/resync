open Lwt.Syntax;
open Caqti_request.Infix;
module type DB = Caqti_lwt.CONNECTION;
module T = Caqti_type;

let todo_caqti_type =
  T.product((id, list_id, text, completed) => {
    Model.Todo.id: id,
    list_id: list_id,
    text: text,
    completed: completed,
  })
    @@ T.proj(T.string, ((todo: Model.Todo.t) => todo.id))
    @@ T.proj(T.string, ((todo: Model.Todo.t) => todo.list_id))
    @@ T.proj(T.string, ((todo: Model.Todo.t) => todo.text))
    @@ T.proj(T.bool, ((todo: Model.Todo.t) => todo.completed))
    @@ T.proj_end;

let get_list = (list_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->* todo_caqti_type)(RealtimeSchema.Queries.GetList.sql)
    );
  (module Db: DB) => {
    let* items_or_error = Db.collect_list(query, list_id);
    let* items = Caqti_lwt.or_fail(items_or_error);
    Lwt.return(Array.of_list(items));
  };
};

let create_list = (list_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->. T.unit)(RealtimeSchema.Mutations.CreateList.sql)
    );
  (module Db: DB) => {
    let* result = Db.exec(query, list_id);
    Caqti_lwt.or_fail(result);
  };
};

let add_todo = args => {
  let (id, list_id, text) = args;
  let query =
    Caqti_request.Infix.(
      (T.t3(T.string, T.string, T.string) ->. T.unit)(
        RealtimeSchema.Mutations.AddTodo.sql,
      )
    );
  (module Db: DB) => {
    let* result = Db.exec(query, (id, list_id, text));
    Caqti_lwt.or_fail(result);
  };
};

let toggle_todo = (todo_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->. T.unit)(RealtimeSchema.Mutations.ToggleTodo.sql)
    );
  (module Db: DB) => {
    let* result = Db.exec(query, todo_id);
    Caqti_lwt.or_fail(result);
  };
};

let remove_todo = (todo_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->. T.unit)(RealtimeSchema.Mutations.RemoveTodo.sql)
    );
  (module Db: DB) => {
    let* result = Db.exec(query, todo_id);
    Caqti_lwt.or_fail(result);
  };
};

let rename_list = (list_id: string, name: string) => {
  let query =
    Caqti_request.Infix.(
      (T.t2(T.string, T.string) ->. T.unit)(
        RealtimeSchema.Mutations.RenameList.sql,
      )
    );
  (module Db: DB) => {
    let* result = Db.exec(query, (list_id, name));
    Caqti_lwt.or_fail(result);
  };
};

let get_list_info = (list_id: string) => {
  let list_caqti_type =
    T.product((id, name) => {
      Model.TodoList.id: id,
      name: name,
    })
      @@ T.proj(T.string, ((list: Model.TodoList.t) => list.id))
      @@ T.proj(T.string, ((list: Model.TodoList.t) => list.name))
      @@ T.proj_end;
  let query =
    Caqti_request.Infix.(
      (T.string ->? list_caqti_type)(
        {sql|SELECT id, name FROM todo_lists WHERE id = $1|sql},
      )
    );
  (module Db: DB) => {
    let* result = Db.find_opt(query, list_id);
    Caqti_lwt.or_fail(result);
  };
};
