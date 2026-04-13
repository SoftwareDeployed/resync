# SQL-first realtime schema annotations

This guide documents the SQL-first workflow used by `realtime-schema`: you write annotated PostgreSQL, and the build generates trigger SQL, schema snapshots, and OCaml metadata.

## The model

- SQL is the source of truth.
- Comments carry the realtime metadata.
- The code generator reads the annotated SQL and emits triggers plus OCaml modules.

## Table annotations

Use line comments immediately before the `CREATE TABLE` statement.

### `@table`

Marks a table for realtime trigger generation.

```sql
-- @table inventory
CREATE TABLE inventory (...);
```

### `@id_column`

Declares the id column if it is not the parser default.

```sql
-- @id_column id
```

### `@composite_key`

Declares a multi-column key.

```sql
-- @composite_key premise_id, period_id
```

### `@broadcast_channel`

Declares how a row maps to a notification channel.

Supported forms include:

- `column=<col>`
- `computed="<sql expr>"`
- `conditional="<sql expr>"`
- `subquery="<sql>"`

### `@broadcast_parent`

Re-broadcasts a parent query when a child table changes.

```sql
-- @broadcast_parent table=inventory query=current_inventory
```

### `@broadcast_to_views`

Broadcasts changes to all channels found in a view table.

```sql
-- @broadcast_to_views table=inventory_view channel=premise_id
```

## Concrete table examples

### Ecommerce inventory

`demos/ecommerce/server/sql/inventory.sql` shows the full table pattern in one file:

```sql
-- @table inventory
-- @id_column id
-- @broadcast_channel column=premise_id
CREATE TABLE inventory (
  id uuid not null default uuidv7() primary key,
  premise_id uuid not null,
  name varchar not null,
  description varchar not null,
  quantity int not null default 0,
  FOREIGN KEY (premise_id) REFERENCES premise(id)
);
```

The matching parent relationship is a bridge table:

```sql
-- @table inventory_period_map
-- @composite_key inventory_id, period_id
-- @broadcast_parent table=inventory query=get_complete_inventory
CREATE TABLE inventory_period_map (
  inventory_id uuid not null,
  period_id uuid not null,
  FOREIGN KEY (inventory_id) REFERENCES inventory(id),
  FOREIGN KEY (period_id) REFERENCES period(id),
  PRIMARY KEY (inventory_id, period_id)
);
```

### Todo-multiplayer realtime tables

`demos/todo-multiplayer/server/sql/schema.sql` uses the same pattern for parent/child lists:

```sql
-- @table todo_lists
-- @id_column id
-- @broadcast_channel column=id

-- @table todos
-- @id_column id
-- @broadcast_channel column=list_id
-- @broadcast_parent table=todo_lists query=get_list
```

## Query and mutation annotations

Use block comments before the SQL statement.

### `@query`

Defines a named read-only query.

```sql
/* @query current_inventory */
SELECT ...;
```

### `@mutation`

Defines a named write operation.

```sql
/* @mutation update_inventory */
UPDATE ...;
```

### `@cache_key`

Adds a cache key to a query.

### `@json_column` / `@json_columns`

Marks `::text` columns that should be re-hydrated as JSON in generated trigger payloads.

### `@handler`

Chooses how the mutation is executed.

- `sql` — run the SQL directly
- `ocaml` — delegate to an OCaml handler

## Concrete query and mutation examples

### Ecommerce queries

`demos/ecommerce/server/sql/inventory.sql` shows a query with both `@cache_key` and `@json_column`:

```sql
/*
@query get_complete_inventory
@cache_key inventory_id
@json_column period_list
SELECT ...
*/
```

The `get_inventory_list` query uses the same JSON normalization pattern without the cache key.

### Todo-multiplayer mutations

`demos/todo-multiplayer/server/sql/schema.sql` demonstrates idempotent writes with `processed_actions`:

```sql
/*
@mutation add_todo
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
*/
```

That guard pattern makes the mutation safe to retry when the client resends the same `action_id`.

## Workflow

1. Write annotated SQL in `server/sql/`.
2. Run the schema/codegen step.
3. Commit the generated `realtime.sql`, snapshot, and migrations.
4. Apply migrations to Postgres.
5. Use the generated OCaml metadata in queries, mutations, and trigger wiring.

## What to look for in generated output

- `@table` turns into trigger functions in `generated/realtime.sql`.
- `@query` and `@mutation` become generated OCaml metadata.
- `@broadcast_parent` creates extra parent-refresh triggers.
- `@json_column` ensures `::text` payloads are rehydrated as JSON before they reach the client.

## Examples in this repo

- `demos/ecommerce/server/sql/inventory.sql`
- `demos/todo-multiplayer/server/sql/schema.sql`

## Related docs

- `docs/realtime-schema.queries.md`
- `docs/realtime-schema.mutations.md`
- `docs/realtime-schema.generated-artifacts.md`
- `docs/realtime.streaming-lifecycle.md`
