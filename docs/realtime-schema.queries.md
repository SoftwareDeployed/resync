# SQL-first queries

This document explains how named realtime queries work in the SQL-first schema system.

## Named queries

Define a query with a `/* @query name */` block comment. The SQL statement must be **inside** the block comment, not after it:

```sql
/*
@query current_inventory
SELECT * FROM inventory WHERE premise_id = $1;
*/
```

**Important**: The parser requires both the annotation and the SQL to be inside the same block comment. A standalone block comment followed by SQL outside the block will not be parsed.

The ecommerce demo uses the same pattern with a richer projection:

```sql
/*
@query get_inventory_list
@json_column period_list
SELECT
  i.description,
  i.id,
  i.name,
  i.quantity,
  i.premise_id,
  COALESCE(JSONB_AGG(TO_JSONB(p.*)) FILTER (WHERE p.id IS NOT NULL), '[]'::jsonb)::text as period_list
FROM inventory i
JOIN inventory_period_map pm ON pm.inventory_id = i.id
JOIN period p ON p.id = pm.period_id
WHERE i.premise_id = $1
GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;
*/
```

## Cache keys

Use `@cache_key` when a query result should be keyed by a stable identifier.

## How queries are used

The code generator turns query annotations into OCaml metadata. Server code then consumes the generated query SQL through Caqti or Dream-side helpers.

Typical flow:

1. Define the query in `server/sql/*.sql`.
2. Regenerate the schema module.
3. Call the generated `Queries.<Name>.sql` from server code.

### Generated query helpers

For every `@query`, the PPX generates a module with Caqti helpers:

```reason
module GetInventoryList = struct
  let name = "get_inventory_list"
  let sql = "SELECT ..."

  type row = {
    id : string;
    premise_id : string;
    name : string;
    description : string;
    quantity : int;
    period_list : string;
  } [@@platform native]

  let caqti_type = Caqti_type.product(...) [@@platform native]
  let param_type = Caqti_type.string [@@platform native]
  let request row_type = Caqti_request.Infix.(param_type ->* row_type)(sql) [@@platform native]
  let find_request row_type = Caqti_request.Infix.(param_type ->? row_type)(sql) [@@platform native]
  let collect (module Db : Caqti_lwt.CONNECTION) row_type params = ... [@@platform native]
  let find_opt (module Db : Caqti_lwt.CONNECTION) row_type params = ... [@@platform native]
end
```

### Example: ecommerce inventory query

Server code calls the generated `collect` directly without hand-writing Caqti requests:

```ocaml
let* item_rows =
  Dream.sql request (fun db ->
    RealtimeSchema.Queries.GetInventoryList.collect
      db
      RealtimeSchema.Queries.GetInventoryList.caqti_type
      premise_id)
in
let items = Array.of_list item_rows
```

### Example: single-row lookup

For queries that return at most one row, use `find_opt`:

```ocaml
let* item_row =
  Dream.sql request (fun db ->
    RealtimeSchema.Queries.GetCompleteInventory.find_opt
      db
      RealtimeSchema.Queries.GetCompleteInventory.caqti_type
      item_id)
in
match item_row with
| None -> ...
| Some row -> ...
```

## Query authoring conventions

- Keep the SQL plain and readable.
- Use positional parameters (`$1`, `$2`, ...) for inputs.
- Keep the result shape stable so the generated decoder stays simple.
- Prefer one named query per logical read.

## Related generated pieces

- `demos/ecommerce/shared/native/dune` and `demos/todo-multiplayer/shared/native/dune` both enable `realtime_schema_ppx`.
- The PPX emits `RealtimeSchema.Queries.<Name>.sql`, which keeps the server code aligned with the SQL file names.

## Example sources

- `demos/ecommerce/server/sql/inventory.sql`
- `demos/todo-multiplayer/server/sql/schema.sql`

## Related docs

- `docs/realtime-schema.sql-annotations.md`
- `docs/realtime-schema.generated-artifacts.md`
- `docs/API_REFERENCE.md`
