CREATE TABLE IF NOT EXISTS messages (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  thread_id uuid NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
  role text NOT NULL,
  content text NOT NULL DEFAULT '',
  created_at timestamptz NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS schema_migrations (version varchar PRIMARY KEY, applied_at timestamp NOT NULL DEFAULT NOW());

DROP TRIGGER IF EXISTS realtime_notify_IF ON IF;
DROP FUNCTION IF EXISTS realtime_notify_IF();

CREATE OR REPLACE FUNCTION realtime_notify_IF()
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
      old_payload := json_build_object('type', 'patch', 'table', 'IF', 'id', to_jsonb(OLD.id), 'action', 'DELETE');
      PERFORM pg_notify(old_channel_name, old_payload::text);
      channel_name := NEW.thread_id::text;
    END IF;
  END IF;
  payload := json_build_object('type', 'patch', 'table', 'IF', 'id', CASE WHEN TG_OP = 'DELETE' THEN to_jsonb(OLD.id) ELSE to_jsonb(NEW.id) END, 'action', TG_OP, 'data', CASE WHEN TG_OP = 'DELETE' THEN NULL ELSE to_jsonb(NEW) END);
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

CREATE TRIGGER realtime_notify_IF
AFTER INSERT OR UPDATE OR DELETE ON IF
FOR EACH ROW EXECUTE FUNCTION realtime_notify_IF();

INSERT INTO schema_migrations (version) VALUES ("20260411050710") ON CONFLICT (version) DO NOTHING;
