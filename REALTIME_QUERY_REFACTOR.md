# Realtime Query Refactor: SQL-First Smart Realtime System

## Overview

This document outlines the architecture for a SQL-first smart realtime system that automatically generates triggers, types, and migrations from annotated PostgreSQL SQL files using a PPX deriver.

## Goals

1. **SQL-First DX**: Use standard PostgreSQL syntax with embedded annotations
2. **Automatic Trigger Generation**: Smart triggers that broadcast complete parent items
3. **Type Safety**: Auto-generated OCaml types from SQL schema
4. **Migration Management**: Combined DDL and trigger updates
5. **Compile-Time Safety**: All code generated via PPX at build time

## Architecture

### 1. SQL Comment Annotation Syntax

#### Basic Table Annotations

```sql
-- @table <table_name>
-- @id_column <column_name>
-- @broadcast_channel column=<column_name>
CREATE TABLE inventory (
  id uuid DEFAULT uuidv7() PRIMARY KEY,
  premise_id uuid NOT NULL REFERENCES premise(id),
  name varchar NOT NULL,
  description varchar NOT NULL,
  quantity int NOT NULL DEFAULT 0
);
```

#### Junction Table with Parent Broadcast

```sql
-- @table inventory_period_map
-- @composite_key inventory_id, period_id
-- @broadcast_parent table=inventory query=get_complete_inventory
--   When this table changes, fetch the complete parent inventory
--   using the named query and broadcast it
CREATE TABLE inventory_period_map (
  inventory_id uuid NOT NULL REFERENCES inventory(id),
  period_id uuid NOT NULL REFERENCES period(id),
  PRIMARY KEY (inventory_id, period_id)
);
```

#### Advanced Broadcast Channel Examples

```sql
-- Simple: Single column channel
-- @broadcast_channel column=premise_id

-- Advanced: Computed channel (concatenate multiple columns)
-- @broadcast_channel computed="CONCAT(organization_id, ':', premise_id)"

-- Advanced: Conditional channel
-- @broadcast_channel conditional="CASE WHEN status = 'active' THEN premise_id ELSE NULL END"

-- Advanced: Subquery channel
-- @broadcast_channel subquery="SELECT premise_id FROM inventory WHERE id = NEW.inventory_id"
```

### 2. Named Query Definitions

```sql
-- @query get_complete_inventory
--   Fetches complete inventory with all pricing periods
-- @cache_key inventory_id
SELECT 
  i.id,
  i.premise_id,
  i.name,
  i.description,
  i.quantity,
  COALESCE(
    JSONB_AGG(TO_JSONB(p.*)) FILTER (WHERE p.id IS NOT NULL),
    '[]'::jsonb
  )::text as period_list
FROM inventory i
LEFT JOIN inventory_period_map pm ON pm.inventory_id = i.id
LEFT JOIN period p ON p.id = pm.period_id
WHERE i.id = $1
GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;

-- @query get_inventory_list
--   Lists all inventory for a premise with pricing
SELECT 
  i.id,
  i.premise_id,
  i.name,
  i.description,
  i.quantity,
  COALESCE(
    JSONB_AGG(TO_JSONB(p.*)) FILTER (WHERE p.id IS NOT NULL),
    '[]'::jsonb
  )::text as period_list
FROM inventory i
LEFT JOIN inventory_period_map pm ON pm.inventory_id = i.id
LEFT JOIN period p ON p.id = pm.period_id
WHERE i.premise_id = $1
GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;
```

## PPX Output

### 1. SchemaTypes.re (Auto-Generated)

```reason
/* Auto-generated from SQL annotations */
/* Generated: 2026-04-02T10:30:00Z */

open Melange_json.Primitives;

/* ========================================================================== */
/* TABLE TYPES                                                                */
/* ========================================================================== */

[@deriving json]
type inventory = {
  id: string,
  premise_id: string,
  name: string,
  description: string,
  quantity: int,
  [@json.option]
  period_list: option(array(Model.Pricing.period)),
};

[@deriving json]
type inventory_period_map = {
  inventory_id: string,
  period_id: string,
};

/* ========================================================================== */
/* NAMED QUERY TYPES                                                          */
/* ========================================================================== */

module Queries = {
  /* get_complete_inventory : $1=string(uuid) -> inventory */
  let get_complete: Caqti_request.t(string, inventory);
  
  /* get_inventory_list : $1=string(uuid) -> array(inventory) */
  let get_list: Caqti_request.t(string, array(inventory));
};

/* ========================================================================== */
/* BROADCAST METADATA                                                         */
/* ========================================================================== */

module Broadcast = {
  type channel_source =
    | Column(string)           /* Simple column reference */
    | Computed(string)         /* SQL expression */
    | Conditional(string)      /* CASE expression */
    | Subquery(string);        /* Subquery returning channel */
  
  type parent_broadcast = {
    parent_table: string,
    query_name: string,        /* Reference to @query */
  };
  
  let inventory_config = {
    table: "inventory",
    id_column: "id",
    channel: Column("premise_id"),
  };
  
  let inventory_period_map_config = {
    table: "inventory_period_map",
    composite_key: [|["inventory_id", "period_id"]|],
    broadcast_parent: Some({
      parent_table: "inventory",
      query_name: "get_complete_inventory",
    }),
  };
};
```

### 2. SchemaTriggers.sql (Auto-Generated)

```sql
-- ============================================================================
-- AUTO-GENERATED TRIGGERS
-- Generated: 2026-04-02T10:30:00Z
-- Source: demos/ecommerce/server/sql/inventory.sql
-- DO NOT EDIT - Regenerate with `dune build`
-- ============================================================================

-- Drop existing triggers (for clean regeneration)
DROP TRIGGER IF EXISTS realtime_notify_inventory ON inventory;
DROP TRIGGER IF EXISTS realtime_notify_inventory_period_map ON inventory_period_map;

DROP FUNCTION IF EXISTS realtime_notify_inventory();
DROP FUNCTION IF EXISTS realtime_notify_inventory_period_map();

-- ============================================================================
-- TRIGGER: inventory (Direct Broadcast)
-- ============================================================================
CREATE OR REPLACE FUNCTION realtime_notify_inventory()
RETURNS TRIGGER AS $$
DECLARE
    payload JSON;
    channel_name TEXT;
    record_id JSONB;
    row_data JSONB;
BEGIN
    -- Build ID
    record_id := to_jsonb(CASE WHEN TG_OP = 'DELETE' THEN OLD.id ELSE NEW.id END);
    
    -- Build row data
    row_data := CASE 
        WHEN TG_OP = 'DELETE' THEN NULL 
        ELSE to_jsonb(NEW) 
    END;
    
    -- Build payload
    payload := json_build_object(
        'type', 'patch',
        'table', 'inventory',
        'id', record_id,
        'action', TG_OP,
        'data', row_data
    );
    
    -- Resolve channel (column=premise_id)
    channel_name := CASE 
        WHEN TG_OP = 'DELETE' THEN OLD.premise_id::text 
        ELSE NEW.premise_id::text 
    END;
    
    -- Broadcast and update timestamp
    IF channel_name IS NOT NULL THEN
        PERFORM pg_notify(channel_name, payload::text);
        UPDATE premise SET updated_at = NOW() WHERE id = channel_name::UUID;
    END IF;
    
    -- Handle premise change on UPDATE
    IF TG_OP = 'UPDATE' AND OLD.premise_id::text != NEW.premise_id::text THEN
        PERFORM pg_notify(OLD.premise_id::text, payload::text);
        UPDATE premise SET updated_at = NOW() WHERE id = OLD.premise_id;
    END IF;
    
    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER realtime_notify_inventory
AFTER INSERT OR UPDATE OR DELETE ON inventory
FOR EACH ROW EXECUTE FUNCTION realtime_notify_inventory();

-- ============================================================================
-- TRIGGER: inventory_period_map (Parent Broadcast)
-- Broadcasts complete parent inventory using @query get_complete_inventory
-- ============================================================================
CREATE OR REPLACE FUNCTION realtime_notify_inventory_period_map()
RETURNS TRIGGER AS $$
DECLARE
    parent_record RECORD;
    complete_payload JSON;
    channel_name TEXT;
    parent_id UUID;
BEGIN
    -- Get parent ID from changed row
    parent_id := CASE 
        WHEN TG_OP = 'DELETE' THEN OLD.inventory_id 
        ELSE NEW.inventory_id 
    END;
    
    -- Fetch complete parent using @query get_complete_inventory
    SELECT 
        i.id,
        i.premise_id,
        i.name,
        i.description,
        i.quantity,
        COALESCE(
            JSONB_AGG(TO_JSONB(p.*)) FILTER (WHERE p.id IS NOT NULL),
            '[]'::jsonb
        ) as period_list
    INTO parent_record
    FROM inventory i
    LEFT JOIN inventory_period_map pm ON pm.inventory_id = i.id
    LEFT JOIN period p ON p.id = pm.period_id
    WHERE i.id = parent_id
    GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;
    
    IF parent_record IS NULL THEN
        -- Parent might have been deleted
        RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
    END IF;
    
    -- Build complete patch payload
    complete_payload := json_build_object(
        'type', 'patch',
        'table', 'inventory',
        'id', to_jsonb(parent_record.id),
        'action', CASE 
            WHEN TG_OP = 'DELETE' THEN 'DELETE' 
            ELSE 'UPDATE' 
        END,
        'data', to_jsonb(parent_record)
    );
    
    -- Resolve channel from parent record
    channel_name := parent_record.premise_id::text;
    
    -- Broadcast and update timestamp
    IF channel_name IS NOT NULL THEN
        UPDATE premise SET updated_at = NOW() WHERE id = channel_name::UUID;
        PERFORM pg_notify(channel_name, complete_payload::text);
    END IF;
    
    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER realtime_notify_inventory_period_map
AFTER INSERT OR UPDATE OR DELETE ON inventory_period_map
FOR EACH ROW EXECUTE FUNCTION realtime_notify_inventory_period_map();
```

### 3. SchemaMigrations.re (Auto-Generated)

```reason
/* Auto-generated migration tracking */
/* Schema hash: a1b2c3d4e5f6... */

let schema_version = "20260402103000";

let migrations = [
  /* Combined DDL + Trigger migration */
  "migration_20260402103000_initial.sql",
];

/* Migration content (generated as separate .sql files) */
module Migration_20260402103000 = {
  let up = {|
-- ============================================================================
-- MIGRATION: 20260402103000_initial
-- ============================================================================

-- Create tables
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE IF NOT EXISTS inventory (
  id uuid DEFAULT uuidv7() PRIMARY KEY,
  premise_id uuid NOT NULL REFERENCES premise(id),
  name varchar NOT NULL,
  description varchar NOT NULL,
  quantity int NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS inventory_period_map (
  inventory_id uuid NOT NULL REFERENCES inventory(id),
  period_id uuid NOT NULL REFERENCES period(id),
  PRIMARY KEY (inventory_id, period_id)
);

-- Create triggers (auto-generated from annotations)
[SchemaTriggers.sql content here]

-- Track schema version
CREATE TABLE IF NOT EXISTS schema_migrations (
  version varchar PRIMARY KEY,
  applied_at timestamp DEFAULT NOW()
);

INSERT INTO schema_migrations (version) VALUES ('20260402103000')
ON CONFLICT (version) DO NOTHING;
|};

  let down = {|
-- Rollback: 20260402103000_initial
DROP TRIGGER IF EXISTS realtime_notify_inventory ON inventory;
DROP TRIGGER IF EXISTS realtime_notify_inventory_period_map ON inventory_period_map;
DROP TABLE IF EXISTS inventory_period_map;
DROP TABLE IF EXISTS inventory;
DELETE FROM schema_migrations WHERE version = '20260402103000';
|};
};
```

### 4. SchemaPatches.re (Auto-Generated Client-Side)

```reason
/* Auto-generated patch decoder for client */
/* Maps all table changes to store updates */

let decodePatch =
  StorePatch.compose([
    /* Direct table: inventory */
    StorePatch.Pg.decodeAs(
      ~table="inventory",
      ~decodeRow=SchemaTypes.inventory_of_json,
      ~insert=data => InventoryUpsert(data),
      ~update=data => InventoryUpsert(data),
      ~delete=id => InventoryDelete(id),
      (),
    ),
    
    /* Child table mapped to parent: inventory_period_map -> inventory */
    StorePatch.Pg.decodeAs(
      ~table="inventory_period_map",
      ~decodeRow=SchemaTypes.inventory_of_json,  /* Returns parent type */
      ~insert=data => InventoryUpsert(data),
      ~update=data => InventoryUpsert(data),
      ~delete=id => InventoryDelete(id),
      (),
    ),
  ]);
```

## Build Integration (Dune)

```scheme
; demos/ecommerce/server/schema/dune

(library
 (name Schema)
 (public_name ecommerce.schema)
 (libraries caqti caqti-lwt)
 (preprocess
  (pps realtime_schema_ppx --sql-dir ../../sql)))

; Generate types from SQL annotations
(rule
 (target SchemaTypes.re)
 (deps (glob_files ../../sql/*.sql))
 (action
  (with-stdout-to %{target}
   (run %{bin:realtime_schema_ppx} --mode=types %{deps}))))

; Generate trigger SQL from annotations
(rule
 (target SchemaTriggers.sql)
 (deps (glob_files ../../sql/*.sql))
 (action
  (with-stdout-to %{target}
   (run %{bin:realtime_schema_ppx} --mode=triggers %{deps}))))

; Generate migrations (DDL + triggers)
(rule
 (target SchemaMigrations.re)
 (deps 
  (glob_files ../../sql/*.sql)
  SchemaTriggers.sql)
 (action
  (with-stdout-to %{target}
   (run %{bin:realtime_schema_ppx} --mode=migrations %{deps}))))

; Generate client patch decoder
(rule
 (target SchemaPatches.re)
 (deps (glob_files ../../sql/*.sql))
 (action
  (with-stdout-to %{target}
   (run %{bin:realtime_schema_ppx} --mode=patches %{deps}))))
```

## Type Inference

### SQL to OCaml Type Mapping

| SQL Type | OCaml Type | Caqti Type |
|----------|-----------|------------|
| `uuid` | `string` | `Caqti_type.string` |
| `varchar`, `text` | `string` | `Caqti_type.string` |
| `int`, `integer` | `int` | `Caqti_type.int` |
| `bigint` | `int64` | `Caqti_type.int64` |
| `boolean` | `bool` | `Caqti_type.bool` |
| `timestamp`, `timestamptz` | `Js.Date.t` | Custom float encoder |
| `json`, `jsonb` | `'a` (generic) | Custom JSON encoder |

### Query Parameter Inference

```sql
-- @query get_by_id
WHERE i.id = $1  -- Inferred: param $1 = string (from column type)
```

## Workflow

1. **Define Schema**: Create/modify `.sql` files with annotations
2. **Build**: Run `dune build` to generate all artifacts
3. **Migrate**: Apply generated migrations to database
4. **Deploy**: Generated code is ready for server and client

## Benefits

- **SQL-First**: Standard PostgreSQL syntax, no new DSL to learn
- **Explicit Control**: Named queries referenced explicitly
- **Type Safety**: Compile-time guarantees for database operations
- **Automatic Sync**: Triggers keep clients updated automatically
- **Complete Items**: Parent items broadcast with all related data
- **Migration Management**: Version-controlled, reproducible schema changes

## Future Enhancements

- Computed broadcast channels with SQL expressions
- Conditional broadcasting based on row values
- Multi-table broadcast aggregations
- Custom serialization formats
- Validation rules in annotations