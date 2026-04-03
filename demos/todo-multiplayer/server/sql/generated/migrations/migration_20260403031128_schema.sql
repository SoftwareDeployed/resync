CREATE TABLE IF NOT EXISTS schema_migrations (version varchar PRIMARY KEY, applied_at timestamp NOT NULL DEFAULT NOW());

DROP TRIGGER IF EXISTS realtime_notify_todo_lists ON todo_lists;
DROP FUNCTION IF EXISTS realtime_notify_todo_lists();

CREATE OR REPLACE FUNCTION realtime_notify_todo_lists()
RETURNS TRIGGER AS $$
DECLARE
  row_id uuid;
  current_record RECORD;
  channel_name TEXT;
  old_channel_name TEXT;
  payload JSON;
  payload_data JSONB;
  old_payload JSON;
  did_broadcast BOOLEAN := FALSE;
BEGIN
  IF TG_OP = 'DELETE' THEN
    channel_name := OLD.id::text;
    payload := json_build_object('type', 'patch', 'table', 'todo_lists', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
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
  IF TG_OP = 'UPDATE' AND OLD.id::text IS DISTINCT FROM NEW.id::text THEN
    old_channel_name := OLD.id::text;
    IF old_channel_name IS NOT NULL THEN
      old_payload := json_build_object('type', 'patch', 'table', 'todo_lists', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
      PERFORM pg_notify(old_channel_name, old_payload::text);
    END IF;
  END IF;
  row_id := NEW.id;
  FOR current_record IN
    SELECT id, list_id, text, completed FROM todos WHERE list_id = row_id ORDER BY created_at
  LOOP
    did_broadcast := TRUE;
    channel_name := current_record.id::text;
    payload_data := to_jsonb(current_record);
    payload := json_build_object('type', 'patch', 'table', 'todo_lists', 'id', to_jsonb(current_record.id), 'action', TG_OP, 'data', payload_data);
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
    channel_name := OLD.id::text;
    IF channel_name IS NOT NULL THEN
      payload := json_build_object('type', 'patch', 'table', 'todo_lists', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
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

CREATE TRIGGER realtime_notify_todo_lists
AFTER INSERT OR UPDATE OR DELETE ON todo_lists
FOR EACH ROW EXECUTE FUNCTION realtime_notify_todo_lists();

DROP TRIGGER IF EXISTS realtime_notify_todos ON todos;
DROP FUNCTION IF EXISTS realtime_notify_todos();

CREATE OR REPLACE FUNCTION realtime_notify_todos()
RETURNS TRIGGER AS $$
DECLARE
  channel_name TEXT;
  old_channel_name TEXT;
  payload JSON;
  old_payload JSON;
BEGIN
  channel_name := CASE WHEN TG_OP = 'DELETE' THEN OLD.list_id::text ELSE NEW.list_id::text END;
  IF TG_OP = 'UPDATE' AND OLD.list_id::text IS DISTINCT FROM NEW.list_id::text THEN
    old_channel_name := OLD.list_id::text;
    IF old_channel_name IS NOT NULL THEN
      old_payload := json_build_object('type', 'patch', 'table', 'todos', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
      PERFORM pg_notify(old_channel_name, old_payload::text);
      channel_name := NEW.list_id::text;
    END IF;
  END IF;
  payload := json_build_object('type', 'patch', 'table', 'todos', 'id', CASE WHEN TG_OP = 'DELETE' THEN to_jsonb(OLD.id) ELSE to_jsonb(NEW.id) END, 'action', TG_OP, 'data', CASE WHEN TG_OP = 'DELETE' THEN NULL ELSE to_jsonb(NEW) END);
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
  RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER realtime_notify_todos
AFTER INSERT OR UPDATE OR DELETE ON todos
FOR EACH ROW EXECUTE FUNCTION realtime_notify_todos();

DROP TRIGGER IF EXISTS realtime_notify_todos_parent ON todos;
DROP FUNCTION IF EXISTS realtime_notify_todos_parent();

CREATE OR REPLACE FUNCTION realtime_notify_todos_parent()
RETURNS TRIGGER AS $$
DECLARE
  parent_row_id uuid;
  parent_record RECORD;
  channel_name TEXT;
  payload JSON;
  payload_data JSONB;
  did_broadcast BOOLEAN := FALSE;
BEGIN
  parent_row_id := CASE WHEN TG_OP = 'DELETE' THEN OLD.list_id ELSE NEW.list_id END;
  IF parent_row_id IS NULL THEN
    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
  END IF;
  FOR parent_record IN
    SELECT id, list_id, text, completed FROM todos WHERE list_id = parent_row_id ORDER BY created_at
  LOOP
    did_broadcast := TRUE;
    channel_name := parent_record.id::text;
    payload_data := to_jsonb(parent_record);
    payload := json_build_object('type', 'patch', 'table', 'todo_lists', 'id', to_jsonb(parent_record.id), 'action', 'UPDATE', 'data', payload_data);
    IF channel_name IS NOT NULL THEN
      PERFORM pg_notify(channel_name, payload::text);
    END IF;
    
  END LOOP;
  IF NOT did_broadcast THEN
    SELECT id::text INTO channel_name FROM todo_lists WHERE id = parent_row_id;
    IF channel_name IS NOT NULL THEN
      payload := json_build_object('type', 'patch', 'table', 'todo_lists', 'id', to_jsonb(parent_row_id), 'action', 'DELETE');
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

CREATE TRIGGER realtime_notify_todos_parent
AFTER INSERT OR UPDATE OR DELETE ON todos
FOR EACH ROW EXECUTE FUNCTION realtime_notify_todos_parent();

INSERT INTO schema_migrations (version) VALUES ("20260403031128") ON CONFLICT (version) DO NOTHING;
