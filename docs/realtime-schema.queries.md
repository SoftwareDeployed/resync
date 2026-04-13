# SQL-first queries

This document explains how named realtime queries work in the SQL-first schema system.

## Named queries

Define a query with a `/* @query name */` block comment immediately before the SQL statement.

```sql
/* @query current_inventory */
SELECT * FROM inventory WHERE premise_id = $1;
```

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

### Example: ecommerce inventory query

`demos/ecommerce/server/src/Database/Inventory.re` wires the generated query into a typed Caqti request:

```reason
let get_list = (premise_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->* inventory_item_caqti_type)(RealtimeSchema.Queries.GetInventoryList.sql)
    );
  (module Db: DB) => {
    let* items_or_error = Db.collect_list(query, premise_id);
    let* items_list = Caqti_lwt.or_fail(items_or_error);
    Lwt.return(Array.of_list(items_list));
  };
};
```

The route entrypoint then uses `Dream.sql(request, Database.Inventory.get_list(premiseId))` to execute it inside the request context.

### Example: single-row lookup

The same file uses the `GetCompleteInventory` query for a single row lookup:

```reason
let get_by_id = (item_id: string) => {
  let query =
    Caqti_request.Infix.(
      (T.string ->? inventory_item_caqti_type)(RealtimeSchema.Queries.GetCompleteInventory.sql)
    );
  (module Db: DB) => {
    let* item_or_error = Db.find_opt(query, item_id);
    Caqti_lwt.or_fail(item_or_error);
  };
};
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
