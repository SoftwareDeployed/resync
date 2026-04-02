CREATE TABLE premise (
  id uuid primary key not null default uuidv7(),
  name varchar not null,
  description varchar not null,
  updated_at timestamp not null default now()
);;

CREATE TABLE inventory (
  id uuid not null default uuidv7() primary key,
  premise_id uuid not null,
  name varchar not null,
  description varchar not null,
  quantity int not null default 0,
  FOREIGN KEY (premise_id) REFERENCES premise(id)
);;

CREATE TABLE period (
    id uuid not null DEFAULT uuidv7() primary key,
    label varchar not null,
    min_value int not null,
    max_value int not null,
    unit unit_enum not null,
    price bigint not null
);;

CREATE TABLE inventory_period_map (
  inventory_id uuid not null,
  period_id uuid not null,
  FOREIGN KEY (inventory_id) REFERENCES inventory(id),
  FOREIGN KEY (period_id) REFERENCES period(id),
  PRIMARY KEY (inventory_id, period_id)
);;

CREATE TABLE premise_route (
  premise_id uuid not null default uuidv7(),
  route_root varchar not null unique,
  FOREIGN KEY (premise_id) REFERENCES premise(id)
);;

CREATE TABLE IF NOT EXISTS schema_migrations (version varchar PRIMARY KEY, applied_at timestamp NOT NULL DEFAULT NOW());

DROP TRIGGER IF EXISTS realtime_notify_inventory ON inventory;
DROP FUNCTION IF EXISTS realtime_notify_inventory();

CREATE OR REPLACE FUNCTION realtime_notify_inventory()
RETURNS TRIGGER AS $$
DECLARE
  row_id uuid;
  current_record RECORD;
  channel_name TEXT;
  old_channel_name TEXT;
  payload JSON;
  old_payload JSON;
  did_broadcast BOOLEAN := FALSE;
BEGIN
  IF TG_OP = 'DELETE' THEN
    channel_name := OLD.premise_id::text;
    payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
    IF channel_name IS NOT NULL THEN
      PERFORM pg_notify(channel_name, payload::text);
    END IF;
    IF channel_name IS NOT NULL THEN
    BEGIN
      UPDATE premise SET updated_at = NOW() WHERE id = channel_name::uuid;
    EXCEPTION WHEN undefined_table THEN
      NULL;
    END;
  END IF;
    RETURN OLD;
  END IF;
  IF TG_OP = 'UPDATE' AND OLD.premise_id::text IS DISTINCT FROM NEW.premise_id::text THEN
    old_channel_name := OLD.premise_id::text;
    IF old_channel_name IS NOT NULL THEN
      old_payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
      PERFORM pg_notify(old_channel_name, old_payload::text);
    END IF;
  END IF;
  row_id := NEW.id;
  FOR current_record IN
    SELECT
  i.description,
  i.id,
  i.name,
  i.quantity,
  i.premise_id,
  COALESCE(
    JSONB_AGG(
      TO_JSONB(p.*)
    ) FILTER (WHERE p.id IS NOT NULL),
    '[]'::jsonb
  )::text as period_list
FROM inventory i
JOIN inventory_period_map pm ON pm.inventory_id = i.id
JOIN period p ON p.id = pm.period_id
WHERE i.id = row_id
GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;
  LOOP
    did_broadcast := TRUE;
    channel_name := current_record.premise_id::text;
    payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(current_record.id), 'action', TG_OP, 'data', to_jsonb(current_record));
    IF channel_name IS NOT NULL THEN
      PERFORM pg_notify(channel_name, payload::text);
    END IF;
    IF channel_name IS NOT NULL THEN
    BEGIN
      UPDATE premise SET updated_at = NOW() WHERE id = channel_name::uuid;
    EXCEPTION WHEN undefined_table THEN
      NULL;
    END;
  END IF;
  END LOOP;
  IF TG_OP = 'UPDATE' AND NOT did_broadcast THEN
    channel_name := OLD.premise_id::text;
    IF channel_name IS NOT NULL THEN
      payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
      PERFORM pg_notify(channel_name, payload::text);
      IF channel_name IS NOT NULL THEN
    BEGIN
      UPDATE premise SET updated_at = NOW() WHERE id = channel_name::uuid;
    EXCEPTION WHEN undefined_table THEN
      NULL;
    END;
  END IF;
    END IF;
  END IF;
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER realtime_notify_inventory
AFTER INSERT OR UPDATE OR DELETE ON inventory
FOR EACH ROW EXECUTE FUNCTION realtime_notify_inventory();

DROP TRIGGER IF EXISTS realtime_notify_period_via_inventory_period_map ON period;
DROP FUNCTION IF EXISTS realtime_notify_period_via_inventory_period_map();

CREATE OR REPLACE FUNCTION realtime_notify_period_via_inventory_period_map()
RETURNS TRIGGER AS $$
DECLARE
  source_row_id uuid;
  parent_row_id uuid;
  parent_record RECORD;
  channel_name TEXT;
  payload JSON;
  did_broadcast BOOLEAN;
BEGIN
  source_row_id := CASE WHEN TG_OP = 'DELETE' THEN OLD.id ELSE NEW.id END;
  IF source_row_id IS NULL THEN
    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
  END IF;
  FOR parent_row_id IN
    SELECT DISTINCT inventory_id FROM inventory_period_map WHERE period_id = source_row_id
  LOOP
    did_broadcast := FALSE;
    FOR parent_record IN
      SELECT
  i.description,
  i.id,
  i.name,
  i.quantity,
  i.premise_id,
  COALESCE(
    JSONB_AGG(
      TO_JSONB(p.*)
    ) FILTER (WHERE p.id IS NOT NULL),
    '[]'::jsonb
  )::text as period_list
FROM inventory i
JOIN inventory_period_map pm ON pm.inventory_id = i.id
JOIN period p ON p.id = pm.period_id
WHERE i.id = parent_row_id
GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;
    LOOP
      did_broadcast := TRUE;
      channel_name := parent_record.premise_id::text;
      payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(parent_record.id), 'action', 'UPDATE', 'data', to_jsonb(parent_record));
      IF channel_name IS NOT NULL THEN
        PERFORM pg_notify(channel_name, payload::text);
      END IF;
      IF channel_name IS NOT NULL THEN
    BEGIN
      UPDATE premise SET updated_at = NOW() WHERE id = parent_record.premise_id::text::uuid;
    EXCEPTION WHEN undefined_table THEN
      NULL;
    END;
  END IF;
    END LOOP;
    IF NOT did_broadcast THEN
      SELECT premise_id::text INTO channel_name FROM inventory WHERE id = parent_row_id;
      IF channel_name IS NOT NULL THEN
        payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(parent_row_id), 'action', 'DELETE');
        PERFORM pg_notify(channel_name, payload::text);
        IF channel_name IS NOT NULL THEN
    BEGIN
      UPDATE premise SET updated_at = NOW() WHERE id = channel_name::uuid;
    EXCEPTION WHEN undefined_table THEN
      NULL;
    END;
  END IF;
      END IF;
    END IF;
  END LOOP;
  RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER realtime_notify_period_via_inventory_period_map
AFTER INSERT OR UPDATE OR DELETE ON period
FOR EACH ROW EXECUTE FUNCTION realtime_notify_period_via_inventory_period_map();

DROP TRIGGER IF EXISTS realtime_notify_inventory_period_map_parent ON inventory_period_map;
DROP FUNCTION IF EXISTS realtime_notify_inventory_period_map_parent();

CREATE OR REPLACE FUNCTION realtime_notify_inventory_period_map_parent()
RETURNS TRIGGER AS $$
DECLARE
  parent_row_id uuid;
  parent_record RECORD;
  channel_name TEXT;
  payload JSON;
  did_broadcast BOOLEAN := FALSE;
BEGIN
  parent_row_id := CASE WHEN TG_OP = 'DELETE' THEN OLD.inventory_id ELSE NEW.inventory_id END;
  IF parent_row_id IS NULL THEN
    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
  END IF;
  FOR parent_record IN
    SELECT
  i.description,
  i.id,
  i.name,
  i.quantity,
  i.premise_id,
  COALESCE(
    JSONB_AGG(
      TO_JSONB(p.*)
    ) FILTER (WHERE p.id IS NOT NULL),
    '[]'::jsonb
  )::text as period_list
FROM inventory i
JOIN inventory_period_map pm ON pm.inventory_id = i.id
JOIN period p ON p.id = pm.period_id
WHERE i.id = parent_row_id
GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;
  LOOP
    did_broadcast := TRUE;
    channel_name := parent_record.premise_id::text;
    payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(parent_record.id), 'action', 'UPDATE', 'data', to_jsonb(parent_record));
    IF channel_name IS NOT NULL THEN
      PERFORM pg_notify(channel_name, payload::text);
    END IF;
    IF channel_name IS NOT NULL THEN
    BEGIN
      UPDATE premise SET updated_at = NOW() WHERE id = parent_record.premise_id::text::uuid;
    EXCEPTION WHEN undefined_table THEN
      NULL;
    END;
  END IF;
  END LOOP;
  IF NOT did_broadcast THEN
    SELECT premise_id::text INTO channel_name FROM inventory WHERE id = parent_row_id;
    IF channel_name IS NOT NULL THEN
      payload := json_build_object('type', 'patch', 'table', 'inventory', 'id', to_jsonb(parent_row_id), 'action', 'DELETE');
      PERFORM pg_notify(channel_name, payload::text);
      IF channel_name IS NOT NULL THEN
    BEGIN
      UPDATE premise SET updated_at = NOW() WHERE id = channel_name::uuid;
    EXCEPTION WHEN undefined_table THEN
      NULL;
    END;
  END IF;
    END IF;
  END IF;
  RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER realtime_notify_inventory_period_map_parent
AFTER INSERT OR UPDATE OR DELETE ON inventory_period_map
FOR EACH ROW EXECUTE FUNCTION realtime_notify_inventory_period_map_parent();

INSERT INTO schema_migrations (version) VALUES ("20260402191210") ON CONFLICT (version) DO NOTHING;
