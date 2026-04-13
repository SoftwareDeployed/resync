# SQL-first mutations

This document explains how realtime mutations are authored in the SQL-first schema system.

## Named mutations

Use `/* @mutation name */` before the SQL statement.

```sql
/* @mutation update_inventory */
UPDATE inventory SET ... WHERE id = $1;
```

The todo-multiplayer demo shows both simple writes and multi-table mutations:

```sql
/* @mutation create_list */
INSERT INTO todo_lists (id) VALUES ($1);

/* @mutation add_todo */
INSERT INTO todos (id, list_id, text)
VALUES ($1::uuid, $2::uuid, $3);
```

All mutations are automatically made idempotent by the middleware. The framework manages a per-mutation `_resync_actions_<name>` table internally; you do not need to write `action_guard` CTEs or touch any system tables.

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

`demos/todo-multiplayer/server/src/server.ml` and `demos/todo-multiplayer/server/src/EntryServer.re` show the generated mutation SQL being consumed from OCaml. The PPX emits a full Caqti request module for each mutation, so you can execute it directly without hand-writing the request boilerplate:

```reason
let create_list = (list_id: string) =>
  (module Db: DB) => RealtimeSchema.Mutations.CreateList.exec((module Db), list_id);
```

For mutations that need custom logic, you can still fall back to `RealtimeSchema.Mutations.<Name>.sql` and build the request yourself, or use the generated `param_type` and `request` values:

```reason
let param_type = RealtimeSchema.Mutations.AddTodo.param_type;
let request = RealtimeSchema.Mutations.AddTodo.request;
```

The framework transparently handles idempotency, so mutations only need to express the logical data change.

### Example: custom mutation dispatcher in llm-chat

`demos/llm-chat/server/src/server.ml` shows the case where you need a custom OCaml handler instead of a direct SQL mutation. It pattern-matches the action kind, writes to the DB using the generated mutation helper, and broadcasts streaming events:

```ocaml
| Ok "send_prompt" ->
    require_thread request thread_id (fun () ->
      let message_id = UUID.make () in
      let assistant_message_id = UUID.make () in
      let* () =
        RealtimeSchema.Mutations.AddMessage.exec
          db
          (message_id, thread_id, "user", prompt)
      in
      Lwt.async (fun () -> stream_ollama ~broadcast_fn ~request ~thread_id ~assistant_message_id ());
      Lwt.return (Ack (Ok ())))
```

That is the practical shape of `@handler ocaml`: the SQL layer gives you the named operation via `RealtimeSchema.Mutations.*.exec`, and the handler can compose extra side effects around it.

## Practical rules

- Prefer one logical write per mutation name.
- The framework handles idempotency automatically; mutations should only contain INSERT/UPDATE/DELETE logic.
- Use explicit identifiers (e.g., client-generated UUIDs) for rows that are created by mutations.

## What to look for in generated output

- `RealtimeSchema.Mutations.<Name>.sql` gives you the literal SQL string for the mutation.
- `RealtimeSchema.Mutations.<Name>.param_type` is the inferred `Caqti_type.t` for the mutation parameters.
- `RealtimeSchema.Mutations.<Name>.request` is the pre-built `Caqti_request.t` for direct use with Caqti.
- `RealtimeSchema.Mutations.<Name>.exec` is a ready-to-use wrapper that runs the mutation against a `Caqti_lwt.CONNECTION` and returns `unit Lwt.t`.
- The middleware automatically checks the per-mutation `_resync_actions_<name>` ledger to prevent duplicate execution.
- If you need multiple statements, express them as separate SQL steps; the transaction wrapper in the middleware ensures atomicity.

## Example sources

- `demos/todo-multiplayer/server/sql/schema.sql` - Primary source for mutation examples
- `demos/ecommerce/server/sql/inventory.sql` - Table annotations (queries, not mutations)

## Related docs

- `docs/realtime-schema.sql-annotations.md`
- `docs/realtime.streaming-lifecycle.md`
- `docs/API_REFERENCE.md`
