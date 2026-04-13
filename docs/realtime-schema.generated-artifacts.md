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

## OCaml metadata

The PPX emits OCaml metadata for tables, queries, and mutations so server code can reference generated names instead of duplicating SQL strings.

That is why server code can call `RealtimeSchema.Queries.GetInventoryList.sql` or `RealtimeSchema.Mutations.CreateList.sql` without hardcoding the SQL in OCaml.

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

- `demos/ecommerce/server/sql/generated/`
- `demos/todo-multiplayer/server/sql/generated/`

## Related docs

- `docs/realtime-schema.sql-annotations.md`
- `docs/realtime-schema.queries.md`
- `docs/realtime-schema.mutations.md`
