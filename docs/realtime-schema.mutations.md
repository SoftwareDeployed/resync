# SQL-first mutations

This document explains how realtime mutations are authored in the SQL-first schema system.

## Named mutations

Use `/* @mutation name */` before the SQL statement.

```sql
/* @mutation update_inventory */
UPDATE inventory SET ... WHERE id = $1;
```

The todo-multiplayer demo uses the same pattern for both simple and idempotent multi-step writes:

```sql
/* @mutation create_list */
INSERT INTO todo_lists (id) VALUES ($1);

/* @mutation add_todo */
WITH action_guard AS (
  INSERT INTO processed_actions (id)
  VALUES ($1::uuid)
  ON CONFLICT DO NOTHING
  RETURNING id
), inserted AS (
  INSERT INTO todos (id, list_id, text)
  SELECT $2::uuid, $3::uuid, $4 FROM action_guard
  RETURNING list_id
)
UPDATE todo_lists
SET updated_at = NOW()
WHERE id IN (SELECT list_id FROM inserted);
```

## Handler modes

### `@handler sql`

The mutation is executed directly as SQL.

### `@handler ocaml`

The mutation is handled in OCaml and may compose multiple SQL operations or call other generated helpers.

## Mutation workflow

1. Client dispatches an action.
2. Server receives a `mutation` frame.
3. The server maps the action to a named mutation.
4. The generated mutation is executed.
5. The runtime emits an ack, patch, or snapshot.

### Example: todo-multiplayer server handler

`demos/todo-multiplayer/server/src/Database/Todo.re` shows the generated mutation SQL being consumed from OCaml:

```reason
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
```

The same file contains hand-written mutations for the idempotent todo actions, which is a good pattern when the mutation needs a guard table or several SQL steps.

### Example: custom mutation dispatcher in llm-chat

`demos/llm-chat/server/src/server.ml` shows the case where you need a custom OCaml handler instead of a direct SQL mutation. It pattern-matches the action kind, writes to the DB, and broadcasts streaming events:

```reason
| Ok "send_prompt" ->
    require_thread request thread_id (fun () ->
      let message_id = UUID.make () in
      let assistant_message_id = UUID.make () in
      let* () = Dream.sql request (Database.Chat.add_message (action_id, message_id, thread_id, "user", prompt)) in
      Lwt.async (fun () -> stream_ollama ~broadcast_fn ~request ~thread_id ~assistant_message_id ());
      Lwt.return (Middleware.Ack (Ok ())))
```

That is the practical shape of `@handler ocaml`: the SQL layer gives you the named operation, and the handler can compose extra side effects around it.

## Practical rules

- Make mutations idempotent where possible.
- Use explicit identifiers for exactly-once semantics.
- Prefer one logical write per mutation name.

## What to look for in generated output

- `RealtimeSchema.Mutations.<Name>.sql` gives you the literal SQL string for the mutation.
- `action_id` should flow from the client through the mutation so retries stay safe.
- If you need multiple statements, keep the guard table pattern close to the mutation definition so the behavior stays obvious.

## Example sources

- `demos/ecommerce/server/sql/inventory.sql`
- `demos/todo-multiplayer/server/sql/schema.sql`

## Related docs

- `docs/realtime-schema.sql-annotations.md`
- `docs/realtime.streaming-lifecycle.md`
- `docs/API_REFERENCE.md`
