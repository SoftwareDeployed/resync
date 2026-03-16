CREATE OR REPLACE FUNCTION notify_row_change() RETURNS TRIGGER AS $$
DECLARE
    payload JSON;
    channel_name TEXT;
    rec RECORD;
    record_id JSONB;
    row_data JSONB;
BEGIN
    -- Determine ID for payload
    IF TG_TABLE_NAME = 'inventory_period_map' THEN
        IF TG_OP = 'DELETE' THEN
            record_id := jsonb_build_object('inventory_id', OLD.inventory_id, 'period_id', OLD.period_id);
        ELSE
            record_id := jsonb_build_object('inventory_id', NEW.inventory_id, 'period_id', NEW.period_id);
        END IF;
    ELSE
        IF TG_OP = 'DELETE' THEN
            record_id := to_jsonb(OLD.id);
        ELSE
            record_id := to_jsonb(NEW.id);
        END IF;
    END IF;

    -- Build row data for INSERT/UPDATE operations
    IF TG_OP = 'DELETE' THEN
        row_data := NULL;
    ELSE
        row_data := to_jsonb(NEW);
    END IF;

    -- Build Payload with full row data
    payload := json_build_object(
        'table', TG_TABLE_NAME,
        'id', record_id,
        'action', TG_OP,
        'data', row_data
    );

    -- Determine channel name (premise_id) and notify
    IF TG_TABLE_NAME = 'premise' THEN
        IF TG_OP = 'DELETE' THEN
            -- XXX Send special notification when premise deleted / no inventory available
            channel_name := OLD.id::text;
        ELSE
            -- Add updated at timestamp, don't add it during delete since the table no longer exists now.
            --UPDATE premise SET updated_at = NOW() WHERE id = channel_name::UUID;
            channel_name := NEW.id::text;
        END IF;
        PERFORM pg_notify(channel_name, payload::text);

    ELSIF TG_TABLE_NAME = 'inventory' THEN
        IF TG_OP = 'DELETE' THEN
            channel_name := OLD.premise_id::text;
        ELSE
            channel_name := NEW.premise_id::text;
        END IF;
        -- Send notification for inventory change
        PERFORM pg_notify(channel_name, payload::text);
        -- Update premise timestamp (this will trigger premise notification too, but we ignore it)
        UPDATE premise SET updated_at = NOW() where id = channel_name::UUID;
        IF TG_OP != 'DELETE' AND OLD.premise_id::text != NEW.premise_id::text THEN
            UPDATE premise SET updated_at = NOW() where id = OLD.premise_id;
            PERFORM pg_notify(OLD.premise_id::text, payload::text);
        END IF;

    ELSIF TG_TABLE_NAME = 'inventory_period_map' THEN
        IF TG_OP = 'DELETE' THEN
            SELECT premise_id::text INTO channel_name FROM inventory WHERE id = OLD.inventory_id;
        ELSE
            SELECT premise_id::text INTO channel_name FROM inventory WHERE id = NEW.inventory_id;
        END IF;

        IF channel_name IS NOT NULL THEN
            -- Add updated at timestamp, will trigger notification
            UPDATE premise SET updated_at = NOW() WHERE id = channel_name::UUID;
            -- PERFORM pg_notify(channel_name, payload::text);
        END IF;

    ELSIF TG_TABLE_NAME = 'period' THEN
        -- Notify all premises that use this period
        FOR rec IN
            SELECT DISTINCT i.premise_id
            FROM inventory i
            JOIN inventory_period_map ipm ON ipm.inventory_id = i.id
            WHERE ipm.period_id = (CASE WHEN TG_OP = 'DELETE' THEN OLD.id ELSE NEW.id END)
        LOOP
            -- Add updated at timestamp, will trigger notification
            UPDATE premise SET updated_at = NOW() WHERE id = rec.premise_id::UUID;
            -- PERFORM pg_notify(rec.premise_id::text, payload::text);
        END LOOP;
    END IF;

    -- Return appropriate row
    IF TG_OP = 'DELETE' THEN
        RETURN OLD;
    ELSE
        RETURN NEW;
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Recreate Triggers
DROP TRIGGER IF EXISTS my_table_change_notify ON inventory;
CREATE TRIGGER my_table_change_notify
AFTER INSERT OR UPDATE OR DELETE ON inventory
FOR EACH ROW EXECUTE FUNCTION notify_row_change();

DROP TRIGGER IF EXISTS my_table_change_notify ON inventory_period_map;
CREATE TRIGGER my_table_change_notify
AFTER INSERT OR UPDATE OR DELETE ON inventory_period_map
FOR EACH ROW EXECUTE FUNCTION notify_row_change();

DROP TRIGGER IF EXISTS my_table_change_notify ON period;
CREATE TRIGGER my_table_change_notify
AFTER INSERT OR UPDATE OR DELETE ON period
FOR EACH ROW EXECUTE FUNCTION notify_row_change();

DROP TRIGGER IF EXISTS my_table_change_notify ON premise;
CREATE TRIGGER my_table_change_notify
AFTER INSERT OR UPDATE OR DELETE ON premise
FOR EACH ROW EXECUTE FUNCTION notify_row_change();
