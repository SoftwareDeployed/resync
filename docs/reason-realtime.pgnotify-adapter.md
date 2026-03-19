# reason-realtime/pgnotify-adapter

PostgreSQL `LISTEN/NOTIFY` adapter for the realtime middleware layer.

## Overview

This adapter bridges PostgreSQL's built-in pub/sub mechanism with the Dream real-time middleware, enabling database-driven real-time updates without polling.

## Features

- **Database-Driven Events**: Real-time updates from PostgreSQL triggers
- **Automatic Reconnection**: Handles database connection drops
- **Channel Filtering**: Subscribe to specific tables or events
- **JSON Payloads**: Full support for JSON/JSONB notifications
- **Transaction Bound**: Notifications tied to commit boundaries

## Installation

Add to your `dune` file:

```lisp
(libraries
  reason_realtime_pgnotify_adapter
  reason_realtime_dream_middleware
  dream
  lwt)
```

## Quick Start

### Basic Setup

```reason
// server.ml
let adapter =
  ReasonRealtimePgNotifyAdapter.create(
    ~databaseUrl="postgres://user:pass@localhost:5432/mydb",
    ~channels=["items_changes", "user_updates"],
  );

let middleware =
  ReasonRealtimeDreamMiddleware.create(
    ~adapter,
    (),
  );

let () =
  Dream.run
  @@ Dream.router([
    Dream.get "/_events" (
      ReasonRealtimeDreamMiddleware.handler(middleware)
    ),
  ]);
```

### Database Trigger Setup

```sql
-- Enable notifications on table changes
CREATE OR REPLACE FUNCTION notify_changes()
RETURNS TRIGGER AS $$
BEGIN
  PERFORM pg_notify(
    TG_TABLE_NAME || '_changes',
    json_build_object(
      'action', TG_OP,
      'table', TG_TABLE_NAME,
      'old', CASE WHEN TG_OP = 'DELETE' THEN row_to_json(OLD) ELSE NULL END,
      'new', CASE WHEN TG_OP IN ('INSERT', 'UPDATE') THEN row_to_json(NEW) ELSE NULL END
    )::text
  );
  RETURN NULL;
END;
$$ LANGUAGE plpgsql;

-- Attach trigger to table
CREATE TRIGGER items_changes_trigger
AFTER INSERT OR UPDATE OR DELETE ON items
FOR EACH ROW EXECUTE FUNCTION notify_changes();
```

## API Reference

### Types

#### `adapter`

```reason
type adapter;
```

The PostgreSQL notify adapter handle.

#### `config`

```reason
type config = {
  databaseUrl: string,
  channels: list(string),
  reconnectInterval: option(int),  // Milliseconds
  maxReconnectAttempts: option(int),
};
```

### Functions

#### `create`

```reason
let create: (~databaseUrl: string, ~channels: list(string)) => adapter;
```

Create a new adapter instance.

**Parameters:**
- `~databaseUrl`: PostgreSQL connection URL
- `~channels`: List of channel names to subscribe to

#### `createWithConfig`

```reason
let createWithConfig: config => adapter;
```

Create adapter with full configuration.

#### `addChannel`

```reason
let addChannel: (adapter, string) => Lwt.t(unit);
```

Dynamically add a channel subscription.

#### `removeChannel`

```reason
let removeChannel: (adapter, string) => Lwt.t(unit);
```

Remove a channel subscription.

#### `stop`

```reason
let stop: adapter => Lwt.t(unit);
```

Stop the adapter and close database connections.

## Configuration

### Connection Options

```reason
let adapter =
  ReasonRealtimePgNotifyAdapter.createWithConfig({
    databaseUrl: "postgres://user:pass@localhost:5432/mydb",
    channels: ["items_changes", "orders_changes"],
    reconnectInterval: Some(5000),       // Reconnect after 5s
    maxReconnectAttempts: Some(10),      // Give up after 10 tries
  });
```

### Environment Variables

```reason
let databaseUrl =
  switch (Sys.getenv_opt("DATABASE_URL")) {
  | Some(url) => url
  | None => "postgres://localhost:5432/mydb"
  };

let adapter =
  ReasonRealtimePgNotifyAdapter.create(
    ~databaseUrl,
    ~channels=["events"],
  );
```

## Trigger Patterns

### Simple Table Notifications

```sql
-- Basic trigger for all changes
CREATE TRIGGER simple_notify
AFTER INSERT OR UPDATE OR DELETE ON my_table
FOR EACH ROW EXECUTE FUNCTION notify_changes();
```

### Conditional Notifications

```sql
-- Only notify on specific changes
CREATE OR REPLACE FUNCTION notify_status_changes()
RETURNS TRIGGER AS $$
BEGIN
  IF OLD.status IS DISTINCT FROM NEW.status THEN
    PERFORM pg_notify(
      'status_changes',
      json_build_object(
        'id', NEW.id,
        'oldStatus', OLD.status,
        'newStatus', NEW.status
      )::text
    );
  END IF;
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER status_change_trigger
AFTER UPDATE ON orders
FOR EACH ROW EXECUTE FUNCTION notify_status_changes();
```

### Rich Payloads

```sql
CREATE OR REPLACE FUNCTION notify_with_metadata()
RETURNS TRIGGER AS $$
DECLARE
  payload json;
BEGIN
  payload := json_build_object(
    'timestamp', NOW(),
    'txid', txid_current(),
    'user', current_user,
    'action', TG_OP,
    'schema', TG_TABLE_SCHEMA,
    'table', TG_TABLE_NAME,
    'data', CASE
      WHEN TG_OP = 'DELETE' THEN row_to_json(OLD)
      ELSE row_to_json(NEW)
    END
  );
  
  PERFORM pg_notify(TG_ARGV[0], payload::text);
  RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER rich_notify
AFTER INSERT OR UPDATE OR DELETE ON items
FOR EACH ROW EXECUTE FUNCTION notify_with_metadata('items_changes');
```

## Message Format

### Standard Payload

```json
{
  "action": "INSERT",
  "table": "items",
  "old": null,
  "new": {
    "id": "123",
    "name": "New Item",
    "price": 29.99
  }
}
```

### Update Payload

```json
{
  "action": "UPDATE",
  "table": "items",
  "old": {
    "id": "123",
    "name": "Old Name",
    "price": 19.99
  },
  "new": {
    "id": "123",
    "name": "New Name",
    "price": 29.99
  }
}
```

### Delete Payload

```json
{
  "action": "DELETE",
  "table": "items",
  "old": {
    "id": "123",
    "name": "Deleted Item"
  },
  "new": null
}
```

## Integration with Store

### Client-Side Patch Handling

```reason
// Store.re
let decodePatch =
  StorePatch.compose([
    StorePatch.Pg.decodeAs(
      ~table="items",
      ~decodeRow=json => {
        open Json.Decode;
        {
          id: json |> field("id", string),
          name: json |> field("name", string),
          price: json |> field("price", float),
        };
      },
      ~insert=data => ItemAdd(data),
      ~update=data => ItemUpdate(data.id, data),
      ~delete=id => ItemDelete(id),
      (),
    ),
  ]);
```

### Server-Side Filtering

```reason
let middleware =
  ReasonRealtimeDreamMiddleware.create(
    ~adapter,
    ~filterMessage=((userId, channel, payload)) => {
      switch (channel) {
      | "items_changes" =>
        // Filter by user permissions
        let itemUserId =
          payload
          |> Json.Decode.field("new", Json.Decode.field("user_id", Json.Decode.string));
        Lwt.return(userId == itemUserId);
      | _ => Lwt.return(false)
      };
    }),
  );
```

## Advanced Patterns

### Multi-Tenant Isolation

```sql
-- Add tenant_id to all tables
ALTER TABLE items ADD COLUMN tenant_id UUID NOT NULL;

-- Create tenant-isolated trigger
CREATE OR REPLACE FUNCTION notify_tenant_changes()
RETURNS TRIGGER AS $$
DECLARE
  tenant_id UUID;
BEGIN
  tenant_id := CASE
    WHEN TG_OP = 'DELETE' THEN OLD.tenant_id
    ELSE NEW.tenant_id
  END;
  
  PERFORM pg_notify(
    'tenant_' || tenant_id::text || '_changes',
    json_build_object(
      'table', TG_TABLE_NAME,
      'action', TG_OP,
      'data', CASE
        WHEN TG_OP = 'DELETE' THEN row_to_json(OLD)
        ELSE row_to_json(NEW)
      END
    )::text
  );
  RETURN NULL;
END;
$$ LANGUAGE plpgsql;
```

### Audit Logging

```sql
CREATE TABLE audit_log (
  id SERIAL PRIMARY KEY,
  table_name TEXT NOT NULL,
  action TEXT NOT NULL,
  old_data JSONB,
  new_data JSONB,
  changed_at TIMESTAMP DEFAULT NOW()
);

CREATE OR REPLACE FUNCTION audit_trigger()
RETURNS TRIGGER AS $$
BEGIN
  INSERT INTO audit_log (table_name, action, old_data, new_data)
  VALUES (
    TG_TABLE_NAME,
    TG_OP,
    CASE WHEN TG_OP != 'INSERT' THEN to_jsonb(OLD) ELSE NULL END,
    CASE WHEN TG_OP != 'DELETE' THEN to_jsonb(NEW) ELSE NULL END
  );
  
  PERFORM pg_notify('audit_changes', json_build_object(
    'table', TG_TABLE_NAME,
    'action', TG_OP
  )::text);
  
  RETURN NULL;
END;
$$ LANGUAGE plpgsql;
```

### Batch Notifications

```sql
-- Reduce notification frequency for high-volume tables
CREATE OR REPLACE FUNCTION notify_batch_changes()
RETURNS TRIGGER AS $$
BEGIN
  -- Use statement-level trigger for batches
  PERFORM pg_notify(
    TG_TABLE_NAME || '_batch_changes',
    json_build_object(
      'table', TG_TABLE_NAME,
      'action', TG_OP,
      'timestamp', NOW()
    )::text
  );
  RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER batch_notify
AFTER INSERT OR UPDATE OR DELETE ON high_volume_table
FOR EACH STATEMENT EXECUTE FUNCTION notify_batch_changes();
```

## Best Practices

### 1. Keep Payloads Small

❌ **Bad:** Including entire rows with large JSONB columns

✅ **Good:** Only include changed fields and IDs

```sql
SELECT json_build_object(
  'id', id,
  'changedFields', (
    SELECT json_object_agg(key, value)
    FROM jsonb_each(to_jsonb(NEW))
    WHERE key IN ('name', 'status', 'updated_at')
  )
);
```

### 2. Use Specific Channels

❌ **Bad:** Single channel for all tables

✅ **Good:** Separate channels per table or event type

```sql
-- Good
pg_notify('orders_status_changes', ...)
pg_notify('inventory_updates', ...)
```

### 3. Handle Connection Failures

```reason
let adapter =
  ReasonRealtimePgNotifyAdapter.createWithConfig({
    databaseUrl,
    channels: ["events"],
    reconnectInterval: Some(1000),
    maxReconnectAttempts: Some(5),
  });

// Monitor connection status
let monitorConnection = () => {
  ReasonRealtimePgNotifyAdapter.onDisconnect(adapter, () => {
    Log.warn("Database connection lost, attempting reconnect...");
  });
  
  ReasonRealtimePgNotifyAdapter.onReconnect(adapter, () => {
    Log.info("Database connection restored");
  });
};
```

### 4. Clean Up Old Connections

```sql
-- Periodically clean up idle connections
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE state = 'idle'
  AND state_change < NOW() - INTERVAL '1 hour';
```

## Troubleshooting

### Notifications Not Received

**Problem:** Clients not getting PostgreSQL notifications

**Solutions:**
1. Verify trigger is created and attached
2. Check channel name matches exactly (case-sensitive)
3. Ensure transaction is committed (notifications sent on COMMIT)
4. Verify database user has TRIGGER permission

**Debug Query:**
```sql
-- Test notification manually
SELECT pg_notify('test_channel', '{"test": true}'::text);

-- Check active listeners
SELECT * FROM pg_stat_activity WHERE wait_event = 'PgNotifySlruLock';

-- Verify trigger exists
SELECT * FROM pg_trigger WHERE tgname = 'your_trigger_name';
```

### Connection Drops

**Problem:** Adapter loses database connection

**Solutions:**
1. Configure reconnection parameters
2. Check database max_connections limit
3. Verify network stability
4. Use connection pooling (PgBouncer)

### Performance Issues

**Problem:** High CPU or notification lag

**Solutions:**
1. Use statement-level triggers for bulk operations
2. Reduce payload size
3. Batch notifications when possible
4. Monitor with:
```sql
SELECT 
  channel,
  count(*),
  max(sent_at) as last_sent
FROM pg_notification_queue
GROUP BY channel;
```

## Monitoring

### Metrics to Track

```reason
// Track adapter metrics
ReasonRealtimePgNotifyAdapter.onMessage(adapter, (channel, payload) => {
  Metrics.increment("pg_notify.received", ~tags=[("channel", channel)]);
  
  let size = payload |> Js.Json.stringify |> String.length;
  Metrics.histogram("pg_notify.payload_size", float_of_int(size));
});
```

### PostgreSQL Monitoring

```sql
-- Active LISTEN connections
SELECT count(*) FROM pg_stat_activity 
WHERE query LIKE '%LISTEN%';

-- Notification queue depth
SELECT count(*) FROM pg_notification_queue;

-- Slowest notifications
SELECT channel, avg(processing_time) as avg_time
FROM notification_log  -- If you maintain one
GROUP BY channel
ORDER BY avg_time DESC;
```

## Examples

### E-commerce Real-time Inventory

See the [ecommerce demo](../demos/ecommerce/) for complete implementation including:
- Inventory change notifications
- Stock level updates
- Order status changes

### Chat Application

```sql
-- Messages table
CREATE TABLE messages (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  room_id UUID NOT NULL,
  user_id UUID NOT NULL,
  content TEXT NOT NULL,
  created_at TIMESTAMP DEFAULT NOW()
);

-- Notify on new messages
CREATE TRIGGER message_notify
AFTER INSERT ON messages
FOR EACH ROW EXECUTE FUNCTION notify_changes();

-- Subscribe to room-specific channel in application
let channels = ["room_" ++ roomId ++ "_messages"];
```

## Related Documentation

- [reason-realtime/dream-middleware](reason-realtime.dream-middleware.md) - WebSocket middleware
- [universal-reason-react/store](universal-reason-react.store.md) - Client store sync
- [PostgreSQL NOTIFY Documentation](https://www.postgresql.org/docs/current/sql-notify.html) - Official docs
