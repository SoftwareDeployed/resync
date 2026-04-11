open Lwt.Syntax;
open Caqti_request.Infix;
module type DB = Caqti_lwt.CONNECTION;
module T = Caqti_type;

let message_caqti_type =
  T.product((id, thread_id, role, content) => {
    Model.Message.id: id,
    thread_id: thread_id,
    role: role,
    content: content,
  })
    @@ T.proj(T.string, ((msg: Model.Message.t) => msg.id))
    @@ T.proj(T.string, ((msg: Model.Message.t) => msg.thread_id))
    @@ T.proj(T.string, ((msg: Model.Message.t) => msg.role))
    @@ T.proj(T.string, ((msg: Model.Message.t) => msg.content))
    @@ T.proj_end;

let thread_caqti_type =
  T.product((id, title, updated_at) => {
    Model.Thread.id: id,
    title: title,
    updated_at: updated_at,
  })
    @@ T.proj(T.string, ((thread: Model.Thread.t) => thread.id))
    @@ T.proj(T.string, ((thread: Model.Thread.t) => thread.title))
    @@ T.proj(T.float, ((thread: Model.Thread.t) => thread.updated_at))
    @@ T.proj_end;

let get_messages = (thread_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->* message_caqti_type)(RealtimeSchema.Queries.GetMessages.sql)
    );
  (module Db: DB) => {
    let* items_or_error = Db.collect_list(query, thread_id);
    let* items = Caqti_lwt.or_fail(items_or_error);
    Lwt.return(Array.of_list(items));
  };
};

let get_thread = (thread_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->? thread_caqti_type)(RealtimeSchema.Queries.GetThread.sql)
    );
  (module Db: DB) => {
    let* result = Db.find_opt(query, thread_id);
    Caqti_lwt.or_fail(result);
  };
};

let get_threads = () => {
  let query =
    Caqti_request.Infix.(
      (T.unit ->* thread_caqti_type)(
        {sql|SELECT id, title, EXTRACT(EPOCH FROM updated_at) * 1000 AS updated_at FROM threads ORDER BY updated_at DESC|sql}
      )
    );
  (module Db: DB) => {
    let* items_or_error = Db.collect_list(query, ());
    let* items = Caqti_lwt.or_fail(items_or_error);
    Lwt.return(Array.of_list(items));
  };
};

let create_thread = (thread_id: string, title: string) => {
  let query =
    Caqti_request.Infix.(
      (T.t2(T.string, T.string) ->. T.unit)(RealtimeSchema.Mutations.CreateThread.sql)
    );
  (module Db: DB) => {
    let* result = Db.exec(query, (thread_id, title));
    Caqti_lwt.or_fail(result);
  };
};

let add_message = args => {
  let (action_id, id, thread_id, role, content) = args;
  let query =
    Caqti_request.Infix.(
      (T.t5(T.string, T.string, T.string, T.string, T.string) ->. T.unit)(
        RealtimeSchema.Mutations.AddMessage.sql
      )
    );
  (module Db: DB) => {
    let* result = Db.exec(query, (action_id, id, thread_id, role, content));
    Caqti_lwt.or_fail(result);
  };
};

let record_thread_view = (thread_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->. T.unit)(
        {sql|INSERT INTO active_thread_views (thread_id) VALUES ($1::uuid) ON CONFLICT DO NOTHING|sql}
      )
    );
  (module Db: DB) => {
    let* result = Db.exec(query, thread_id);
    Caqti_lwt.or_fail(result);
  };
};

let delete_thread = (thread_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->. T.unit)(
        {sql|DELETE FROM threads WHERE id = $1::uuid|sql}
      )
    );
  (module Db: DB) => {
    let* result = Db.exec(query, thread_id);
    Caqti_lwt.or_fail(result);
  };
};
