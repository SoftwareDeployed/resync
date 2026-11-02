# Generated artifacts

Annotated SQL produces generated trigger SQL, plus OCaml query/mutation modules at compile time.

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

The exact body is longer, but that is the essential shape: build a patch payload, send it to the row's channel, and let the realtime adapter fan it out.

Regenerate with the per-demo script (e.g. `pnpm run schema:generate` for ecommerce), which only writes `realtime.sql`. There is no snapshot or migration output; schema changes are applied directly (schema.sql + realtime.sql) by the demo init scripts, which drop and recreate the demo database.

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

The PPX emits OCaml modules for queries and mutations so server code can reference generated names instead of duplicating SQL strings.

Server code can call `RealtimeSchema.Queries.GetInventoryList.sql` or `RealtimeSchema.Mutations.CreateList.sql` without hardcoding the SQL in OCaml.

For mutations, the PPX also emits a complete Caqti module: `param_type`, `request`, and `exec`. This lets you run schema-defined mutations directly without hand-writing Caqti request boilerplate.

## Workflow

1. Edit annotated SQL.
2. Regenerate `realtime.sql` (`pnpm run <demo>:schema:generate`).
3. Review the generated SQL.
4. Re-apply schema + triggers to Postgres with the demo init script (e.g. `pnpm run ecommerce:db:init`).
5. Commit the source SQL plus generated triggers together.

## Concrete repository examples

- `demos/ecommerce/server/sql/generated/realtime.sql` — inventory triggers and parent rebroadcast triggers.
- `demos/todo-multiplayer/server/sql/generated/realtime.sql` — todo list and todo child-table triggers.
- `demos/ecommerce/shared/native/dune` — enables `realtime_schema_ppx` for the shared schema module.
- `demos/todo-multiplayer/shared/native/dune` — same PPX setup for the multiplayer demo.

## Related docs

- `docs/realtime-schema.sql-annotations.md`
- `docs/realtime-schema.queries.md`
- `docs/realtime-schema.mutations.md`
