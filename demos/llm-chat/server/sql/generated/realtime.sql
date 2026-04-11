DROP TRIGGER IF EXISTS realtime_notify_threads ON threads;
DROP FUNCTION IF EXISTS realtime_notify_threads();

CREATE OR REPLACE FUNCTION realtime_notify_threads()
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
    payload := json_build_object('type', 'patch', 'table', 'threads', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
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
      old_payload := json_build_object('type', 'patch', 'table', 'threads', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
      PERFORM pg_notify(old_channel_name, old_payload::text);
    END IF;
  END IF;
  row_id := NEW.id;
  FOR current_record IN
    SELECT id, title, EXTRACT(EPOCH FROM created_at) * 1000 AS created_at, EXTRACT(EPOCH FROM updated_at) * 1000 AS updated_at FROM threads WHERE id = row_id
  LOOP
    did_broadcast := TRUE;
    channel_name := current_record.id::text;
    payload_data := to_jsonb(current_record);
    payload := json_build_object('type', 'patch', 'table', 'threads', 'id', to_jsonb(current_record.id), 'action', TG_OP, 'data', payload_data);
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
      payload := json_build_object('type', 'patch', 'table', 'threads', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
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

CREATE TRIGGER realtime_notify_threads
AFTER INSERT OR UPDATE OR DELETE ON threads
FOR EACH ROW EXECUTE FUNCTION realtime_notify_threads();

DROP TRIGGER IF EXISTS realtime_notify_messages ON messages;
DROP FUNCTION IF EXISTS realtime_notify_messages();

CREATE OR REPLACE FUNCTION realtime_notify_messages()
RETURNS TRIGGER AS $$
DECLARE
  channel_name TEXT;
  old_channel_name TEXT;
  payload JSON;
  old_payload JSON;
BEGIN
  channel_name := CASE WHEN TG_OP = 'DELETE' THEN OLD.thread_id::text ELSE NEW.thread_id::text END;
  IF TG_OP = 'UPDATE' AND OLD.thread_id::text IS DISTINCT FROM NEW.thread_id::text THEN
    old_channel_name := OLD.thread_id::text;
    IF old_channel_name IS NOT NULL THEN
      old_payload := json_build_object('type', 'patch', 'table', 'messages', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
      PERFORM pg_notify(old_channel_name, old_payload::text);
      channel_name := NEW.thread_id::text;
    END IF;
  END IF;
  payload := json_build_object('type', 'patch', 'table', 'messages', 'id', CASE WHEN TG_OP = 'DELETE' THEN to_jsonb(OLD.id) ELSE to_jsonb(NEW.id) END, 'action', TG_OP, 'data', CASE WHEN TG_OP = 'DELETE' THEN NULL ELSE to_jsonb(NEW) END);
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

CREATE TRIGGER realtime_notify_messages
AFTER INSERT OR UPDATE OR DELETE ON messages
FOR EACH ROW EXECUTE FUNCTION realtime_notify_messages();

DROP TRIGGER IF EXISTS realtime_notify_messages_parent ON messages;
DROP FUNCTION IF EXISTS realtime_notify_messages_parent();

CREATE OR REPLACE FUNCTION realtime_notify_messages_parent()
RETURNS TRIGGER AS $$
DECLARE
  parent_row_id uuid;
  parent_record RECORD;
  channel_name TEXT;
  payload JSON;
  payload_data JSONB;
  did_broadcast BOOLEAN := FALSE;
BEGIN
  parent_row_id := CASE WHEN TG_OP = 'DELETE' THEN OLD.thread_id ELSE NEW.thread_id END;
  IF parent_row_id IS NULL THEN
    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
  END IF;
  FOR parent_record IN
    SELECT id, title, EXTRACT(EPOCH FROM created_at) * 1000 AS created_at, EXTRACT(EPOCH FROM updated_at) * 1000 AS updated_at FROM threads WHERE id = parent_row_id
  LOOP
    did_broadcast := TRUE;
    channel_name := parent_record.id::text;
    payload_data := to_jsonb(parent_record);
    payload := json_build_object('type', 'patch', 'table', 'threads', 'id', to_jsonb(parent_record.id), 'action', 'UPDATE', 'data', payload_data);
    IF channel_name IS NOT NULL THEN
      PERFORM pg_notify(channel_name, payload::text);
    END IF;
    
  END LOOP;
  IF NOT did_broadcast THEN
    SELECT id::text INTO channel_name FROM threads WHERE id = parent_row_id;
    IF channel_name IS NOT NULL THEN
      payload := json_build_object('type', 'patch', 'table', 'threads', 'id', to_jsonb(parent_row_id), 'action', 'DELETE');
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

CREATE TRIGGER realtime_notify_messages_parent
AFTER INSERT OR UPDATE OR DELETE ON messages
FOR EACH ROW EXECUTE FUNCTION realtime_notify_messages_parent();
