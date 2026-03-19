# reason-realtime/pgnotify-adapter

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.

PostgreSQL `LISTEN/NOTIFY` adapter for the Dream real-time middleware.

## Overview

This adapter keeps a PostgreSQL connection and subscribes/unsubscribes handlers per
channel. Notifications are polled from the connection and dispatched to channel
handlers as raw payload strings.

## Features

- **Database-driven updates** from PostgreSQL `NOTIFY`
- **Channel lifecycle** via explicit subscribe/unsubscribe
- **Raw payload forwarding** for full application-level control
- **Simple adapter lifecycle** with `start` and `stop`

## Installation

Add the dependency in your `dune` file:

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
// server.re
let resolve_subscription request selection =
  /* map a client command to a channel */
  Lwt.return(Some(selection));

let load_snapshot request channel = {
  Lwt.return(
    Printf.sprintf("{\"type\":\"snapshot\",\"channel\":\"%s\",\"payload\":{}}", channel)
  );
};

let pg_adapter =
  Pgnotify_adapter.create(~db_uri="postgres://user:pass@localhost:5432/mydb", ());

let adapter = Adapter.pack((module Pgnotify_adapter), pg_adapter);

let middleware =
  Middleware.create(
    ~adapter,
    ~resolve_subscription,
    ~load_snapshot,
  );

let () =
  /* Start polling for LISTEN events */
  let _ = Lwt.async(() => Adapter.start(adapter));

  Dream.run
  @@ Dream.router([
    Dream.get "/_events" (Middleware.route("/_events", middleware)),
    Dream.get "/" (_ => Dream.html("Hello")),
  ]);
```

> Note: `Adapter.start` is required so notifications are consumed and pushed into
> registered handlers.

## API Reference

### Types

#### `Pgnotify_adapter.t`

```reason
type t;
```

PostgreSQL adapter handle.

### Functions

#### `Pgnotify_adapter.create`

```reason
let create: (~db_uri: string, unit) => t;
```

Creates a new adapter using a PostgreSQL connection URI.

#### `Pgnotify_adapter.start`

```reason
let start: t => Lwt.t(unit);
```

Starts the internal notification polling loop.

#### `Pgnotify_adapter.stop`

```reason
let stop: t => Lwt.t(unit);
```

Stops polling and closes the database connection.

#### `Pgnotify_adapter.subscribe`

```reason
let subscribe: (t, ~channel: string, ~handler: (string => unit Lwt.t)) => Lwt.t(unit);
```

Registers a handler for a channel and opens `LISTEN` for that channel.

#### `Pgnotify_adapter.unsubscribe`

```reason
let unsubscribe: (t, ~channel: string) => Lwt.t(unit);
```

Unregisters all handlers for a channel and issues `UNLISTEN`.

## Database Trigger Setup

```sql
CREATE OR REPLACE FUNCTION notify_changes()
RETURNS TRIGGER AS $$
BEGIN
  PERFORM pg_notify(
    TG_TABLE_NAME || '_changes',
    json_build_object(
      'table', TG_TABLE_NAME,
      'action', TG_OP,
      'id', CASE WHEN TG_OP = 'DELETE' THEN OLD.id ELSE NEW.id END,
      'data', CASE
        WHEN TG_OP = 'DELETE' THEN NULL
        ELSE to_jsonb(NEW)
      END
    )::text
  );
  RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER items_changes_trigger
AFTER INSERT OR UPDATE OR DELETE ON items
FOR EACH ROW EXECUTE FUNCTION notify_changes();
```

## Message Format

Payloads are passed through from PostgreSQL as a raw text string and parsed into:

- `table` (string)
- `id` (JSON value)
- `action` (`INSERT` | `UPDATE` | `DELETE`)
- `data` (optional JSON object)

### Insert/Update

```json
{
  "table": "inventory",
  "action": "INSERT",
  "id": "123",
  "data": {"id": "123", "name": "New Item", "price": 29.99}
}
```

### Delete

```json
{
  "table": "inventory",
  "action": "DELETE",
  "id": "123"
}
```

The adapter currently forwards updates only for the `inventory` table; unsupported
tables are ignored.

## Integration Notes

- Route authorization and tenancy checks belong in your `resolve_subscription`
  and `load_snapshot` callbacks in `reason-realtime/dream-middleware`.
- The adapter only handles transport-level channel delivery; application-level
  filtering should be done before payloads are sent to clients.

## Troubleshooting

### Notifications Not Received

1. Ensure `Adapter.start` is running.
2. Confirm trigger payload shape includes a `table`, `action`, and `id` field.
3. Confirm channel names match between trigger output and client `select` commands.
4. Verify PostgreSQL user permissions for LISTEN/NOTIFY and table triggers.

### Connection Errors

1. Verify `db_uri` values (user, password, host, database).
2. Confirm PostgreSQL is reachable from the app host.
3. Check `pg_stat_activity` for active LISTEN sessions.

### Performance

1. Keep payloads small.
2. Use specific channels for high-volume events.
3. Consider filtering/transforming payloads at trigger time.

## Related Documentation

- [reason-realtime/dream-middleware](reason-realtime.dream-middleware.md) - websocket middleware
- [universal-reason-react/store](universal-reason-react.store.md) - store sync patterns
- [PostgreSQL NOTIFY Documentation](https://www.postgresql.org/docs/current/sql-notify.html)
