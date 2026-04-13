# Generated artifacts

Annotated SQL produces several generated outputs.

## `generated/realtime.sql`

Contains the trigger functions and `NOTIFY` wiring generated from annotated tables.

For example, the ecommerce demo generates a trigger like this for `inventory`:

```sql
CREATE OR REPLACE FUNCTION realtime_notify_inventory()
RETURNS TRIGGER AS $$
BEGIN
  payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(NEW.id), 'action', TG_OP, 'data', payload_data);
  PERFORM pg_notify(channel_name, payload::text);
END;
$$ LANGUAGE plpgsql;
```

The exact body is longer, but that is the essential shape: build a patch payload, send it to the row’s channel, and let the realtime adapter fan it out.

## `generated/schema_snapshot.json`

Stores schema metadata used to detect changes and generate migrations.

This snapshot is what lets the generator decide whether it should emit a fresh migration or just regenerate the existing artifacts.

## `generated/migrations/*.sql`

Incremental SQL migrations generated when the schema changes.

The migrations are still plain SQL, so you can inspect them before applying them with `psql` or your demo init script.

**Note:** Not all demos have migrations. The `llm-chat` demo uses migrations (see `demos/llm-chat/server/sql/generated/migrations/`), while the `ecommerce` and `todo-multiplayer` demos use direct schema application.

## Trigger Functions

Each annotated table generates a trigger function. For example:

**Ecommerce demo triggers:**
- `realtime_notify_inventory` - inventory table changes
- `realtime_notify_period_via_inventory_period_map` - period propagation
- `realtime_notify_inventory_period_map_parent` - parent rebroadcast

**Todo-multiplayer demo triggers:**
- `realtime_notify_todo_lists` - todo_lists table changes
- `realtime_notify_todos` - todos table changes
- `realtime_notify_todos_parent` - parent rebroadcast to todo_lists

## OCaml metadata

The PPX emits OCaml metadata for tables, queries, and mutations so server code can reference generated names instead of duplicating SQL strings.

That is why server code can call `RealtimeSchema.Queries.GetInventoryList.sql` or `RealtimeSchema.Mutations.CreateList.sql` without hardcoding the SQL in OCaml.

For mutations, the PPX also emits a complete Caqti module: `param_type`, `request`, and `exec`. This lets you run schema-defined mutations directly without hand-writing Caqti request boilerplate.

## Workflow

1. Edit annotated SQL.
2. Regenerate outputs.
3. Review the generated SQL and snapshot.
4. Apply the migration SQL to Postgres.
5. Commit the source SQL plus generated artifacts together.

## Concrete repository examples

- `demos/ecommerce/server/sql/generated/realtime.sql` — inventory triggers and parent rebroadcast triggers.
- `demos/todo-multiplayer/server/sql/generated/realtime.sql` — todo list and todo child-table triggers.
- `demos/ecommerce/shared/native/dune` — enables `realtime_schema_ppx` for the shared schema module.
- `demos/todo-multiplayer/shared/native/dune` — same PPX setup for the multiplayer demo.

## Examples in this repo

- `demos/ecommerce/server/sql/generated/` - triggers and snapshot
- `demos/todo-multiplayer/server/sql/generated/` - triggers and snapshot
- `demos/llm-chat/server/sql/generated/` - triggers, snapshot, and migrations

## Related docs

- `docs/realtime-schema.sql-annotations.md`
- `docs/realtime-schema.queries.md`
- `docs/realtime-schema.mutations.md`
