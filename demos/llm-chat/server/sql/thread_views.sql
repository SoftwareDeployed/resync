CREATE OR REPLACE FUNCTION realtime_notify_threads_to_views()
RETURNS TRIGGER AS $$
DECLARE
  view_record RECORD;
  payload JSON;
  payload_data JSONB;
BEGIN
  payload_data := to_jsonb(NEW);
  payload := json_build_object(
    'type', 'patch',
    'table', 'threads',
    'id', to_jsonb(NEW.id),
    'action', 'INSERT',
    'data', payload_data
  );
  FOR view_record IN
    SELECT DISTINCT thread_id FROM active_thread_views
  LOOP
    PERFORM pg_notify(view_record.thread_id::text, payload::text);
  END LOOP;
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER realtime_notify_threads_to_views
AFTER INSERT ON threads
FOR EACH ROW EXECUTE FUNCTION realtime_notify_threads_to_views();
