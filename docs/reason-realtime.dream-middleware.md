# reason-realtime/dream-middleware

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.


Dream middleware and WebSocket infrastructure for real-time server-to-client updates.

## Overview

This package provides the server-side infrastructure for real-time updates in Universal Reason React applications. It handles WebSocket connections, message routing, and integration with Dream's request handling.

## Features

- WebSocket endpoint for client connections
- Middleware for request authentication and validation
- Message routing and delivery
- Integration with PostgreSQL LISTEN/NOTIFY
- Automatic reconnection handling

## Installation

Add to your `dune` file:

```lisp
(libraries
  reason_realtime_dream_middleware
  dream
  lwt)
```

## Quick Start

### Basic Setup

```reason
// server.ml
open Dream;

let () =
  Dream.run
  @@ Dream.logger
  @@ Dream.router([
    // Real-time events endpoint
    Dream.get "/_events" (
      ReasonRealtimeDreamMiddleware.handler(
        ~authenticate=(request => {
          // Validate JWT or session
          switch (Dream.header(request, "Authorization")) {
          | Some(token) => validateToken(token)
          | None => Lwt.return(None)
          };
        }),
        ~onConnect=(userId => {
          Log.info("User connected: " ++ userId);
          Lwt.return_unit;
        }),
        ~onDisconnect=(userId => {
          Log.info("User disconnected: " ++ userId);
          Lwt.return_unit;
        }),
      )
    ),
    
    // Your app routes
    Dream.get "/**" (
      UniversalRouterDream.handler(~app=EntryServer.app)
    ),
  ]);
```

### With PostgreSQL Notifications

```reason
// server.ml
let pgNotifyAdapter =
  ReasonRealtimePgNotifyAdapter.create(
    ~databaseUrl="postgres://user:pass@localhost/db",
    ~channels=["items_changes", "user_updates"],
  );

let realtimeMiddleware =
  ReasonRealtimeDreamMiddleware.create(
    ~adapter=pgNotifyAdapter,
    ~filterMessage=((userId, channel, payload)) => {
      // Filter messages based on user permissions
      switch (channel) {
      | "items_changes" => Lwt.return(true)  // Public
      | "user_updates" =>
        let targetUserId = payload |> Json.Decode.field("userId", Json.Decode.string);
        Lwt.return(userId == targetUserId);  // Private
      | _ => Lwt.return(false)
      };
    }),
  );

let () =
  Dream.run
  @@ Dream.logger
  @@ Dream.router([
    Dream.get "/_events" (ReasonRealtimeDreamMiddleware.handler(realtimeMiddleware)),
  ]);
```

## API Reference

### Types

#### `handler`

```reason
type handler = Dream.handler;
```

Dream-compatible request handler for WebSocket connections.

#### `config`

```reason
type config = {
  authenticate: Dream.request => Lwt.t(option(string)),  // Returns userId or None
  onConnect: string => Lwt.t(unit),                      // userId => unit
  onDisconnect: string => Lwt.t(unit),                   // userId => unit
  heartbeatInterval: option(int),                        // Milliseconds
  maxConnections: option(int),                           // Per-user limit
};
```

### Functions

#### `create`

```reason
let create: (~adapter: adapter=?, config) => t;
```

Create a new middleware instance with the given configuration.

**Parameters:**
- `~adapter`: Optional real-time adapter (e.g., PostgreSQL notify adapter)
- `config`: Configuration record with authentication and event handlers

#### `handler`

```reason
let handler: t => Dream.handler;
```

Convert the middleware to a Dream-compatible handler.

#### `broadcast`

```reason
let broadcast: (t, ~channel: string, ~payload: Js.Json.t) => Lwt.t(unit);
```

Broadcast a message to all connected clients on a channel.

#### `sendToUser`

```reason
let sendToUser: (t, ~userId: string, ~channel: string, ~payload: Js.Json.t) => Lwt.t(unit);
```

Send a message to a specific user.

#### `sendToUsers`

```reason
let sendToUsers: (t, ~userIds: list(string), ~channel: string, ~payload: Js.Json.t) => Lwt.t(unit);
```

Send a message to multiple specific users.

### Authentication

#### JWT Example

```reason
let authenticate = (request: Dream.request) => {
  switch (Dream.header(request, "Authorization")) {
  | Some("Bearer " ++ token) =>
    switch (Jwt.verify(token, secret)) {
    | Ok(payload) =>
      let userId = payload |> Json.Decode.field("sub", Json.Decode.string);
      Lwt.return(Some(userId));
    | Error(_) => Lwt.return(None)
    }
  | _ => Lwt.return(None)
  };
};
```

#### Session Example

```reason
let authenticate = (request: Dream.request) => {
  switch (Dream.session_field(request, "user_id")) {
  | Some(userId) => Lwt.return(Some(userId));
  | None => Lwt.return(None)
  };
};
```

## Message Protocol

### Client → Server

```json
{
  "type": "subscribe",
  "channel": "items_changes",
  "filter": { "category": "electronics" }
}
```

```json
{
  "type": "unsubscribe",
  "channel": "items_changes"
}
```

```json
{
  "type": "ping"
}
```

### Server → Client

```json
{
  "type": "patch",
  "channel": "items_changes",
  "payload": {
    "table": "items",
    "action": "insert",
    "data": { "id": "123", "name": "New Item" }
  }
}
```

```json
{
  "type": "snapshot",
  "channel": "items_changes",
  "payload": {
    "timestamp": "2024-01-15T10:30:00Z",
    "data": [...]
  }
}
```

```json
{
  "type": "pong"
}
```

## Advanced Configuration

### Custom Message Filtering

```reason
let filterMessage = ((userId, channel, payload)) => {
  switch (channel) {
  | "private_messages" =>
    let recipientId = payload |> Json.Decode.field("recipientId", Json.Decode.string);
    Lwt.return(userId == recipientId);
  | "team_updates" =>
    let* userTeam = Database.getUserTeam(userId);
    let updateTeam = payload |> Json.Decode.field("teamId", Json.Decode.string);
    Lwt.return(userTeam == updateTeam);
  | _ => Lwt.return(true)
  };
};

let middleware =
  ReasonRealtimeDreamMiddleware.create(
    ~adapter,
    ~filterMessage,
  );
```

### Rate Limiting

```reason
let rateLimiter = RateLimiter.create(
  ~maxRequests=100,
  ~windowMs=60000,
);

let authenticate = (request) => {
  let* userId = authenticateUser(request);
  switch (userId) {
  | Some(id) =>
    if (RateLimiter.check(rateLimiter, id)) {
      Lwt.return(Some(id));
    } else {
      Lwt.return(None);  // Rate limited
    }
  | None => Lwt.return(None)
  };
};
```

### Connection Management

```reason
let config = {
  authenticate,
  onConnect: (userId) => {
    Metrics.increment("websocket.connections.active");
    Log.info(f"User %s connected", userId);
    Lwt.return_unit;
  },
  onDisconnect: (userId) => {
    Metrics.decrement("websocket.connections.active");
    Log.info(f"User %s disconnected", userId);
    Lwt.return_unit;
  },
  heartbeatInterval: Some(30000),  // 30 seconds
  maxConnections: Some(5),         // Max 5 connections per user
};
```

## Error Handling

### Connection Errors

```reason
let onError = ((userId, error)) => {
  Log.error(f"WebSocket error for user %s: %s", userId, error);
  Metrics.increment("websocket.errors");
  Lwt.return_unit;
};

let middleware =
  ReasonRealtimeDreamMiddleware.create(
    ~onError,
    ~adapter,
  );
```

### Graceful Degradation

```reason
let authenticate = (request) => {
  try%lwt {
    let* userId = validateSession(request);
    Lwt.return(userId);
  } catch {
  | _ =>
    // Allow connection without authentication for public data
    Lwt.return(Some("anonymous"))
  };
};
```

## Best Practices

### 1. Validate All Messages

Always validate incoming messages before processing:

```reason
let validateSubscribe = (json) => {
  open Json.Decode;
  {
    channel: json |> field("channel", string),
    filter: json |> optional(field("filter", object_)),
  };
};
```

### 2. Use Channels Effectively

Group related data into channels:

```reason
channels: [
  "items:all",           // All item changes
  "items:category:123",  // Category-specific
  "items:user:456",      // User's items
];
```

### 3. Handle Reconnections

Implement exponential backoff on the client:

```javascript
// Client-side pseudocode
let reconnectDelay = 1000;
const maxDelay = 30000;

socket.onclose = () => {
  setTimeout(() => connect(), reconnectDelay);
  reconnectDelay = Math.min(reconnectDelay * 2, maxDelay);
};
```

### 4. Monitor Connections

Track connection metrics:

```reason
let onConnect = (userId) => {
  Metrics.gauge("websocket.connections.total", getConnectionCount());
  Metrics.increment("websocket.connections.new");
  Lwt.return_unit;
};
```

### 5. Secure Private Channels

Always validate channel access:

```reason
let filterMessage = ((userId, channel, payload)) => {
  if (String.starts_with(channel, "private:")) {
    let allowedUsers = payload |> getAllowedUsers;
    Lwt.return(List.mem(userId, allowedUsers));
  } else {
    Lwt.return(true);
  };
};
```

## Troubleshooting

### Connection Refused

**Problem:** Clients can't connect to `/_events`

**Solutions:**
1. Verify Dream router includes the events endpoint
2. Check firewall rules allow WebSocket connections
3. Ensure no reverse proxy is blocking the endpoint

### Messages Not Received

**Problem:** Clients connected but not receiving updates

**Solutions:**
1. Check adapter is properly connected to data source
2. Verify `filterMessage` isn't blocking valid messages
3. Ensure client subscribed to the correct channel

### High Memory Usage

**Problem:** Server memory grows over time

**Solutions:**
1. Implement connection limits per user
2. Set appropriate heartbeat intervals
3. Clean up disconnected sessions properly
4. Monitor for memory leaks in message handlers

### Authentication Failures

**Problem:** Users can't authenticate WebSocket connections

**Solutions:**
1. Check authentication logic in Dream handlers
2. Ensure cookies/sessions are properly passed
3. Verify CORS settings allow credentials

## Integration Examples

### With Store Sync

See [universal-reason-react/store](universal-reason-react.store.md) for how the middleware integrates with the store's real-time synchronization.

### With PostgreSQL

See [reason-realtime/pgnotify-adapter](reason-realtime.pgnotify-adapter.md) for PostgreSQL LISTEN/NOTIFY integration.

### Custom Adapter

```reason
module CustomAdapter = {
  type t = { /* your adapter state */ };
  
  let create = () => { ... };
  
  let subscribe = (t, ~channel, ~callback) => {
    // Subscribe to your data source
    Lwt.return_unit;
  };
  
  let unsubscribe = (t, ~channel) => {
    // Cleanup subscription
    Lwt.return_unit;
  };
};

let middleware =
  ReasonRealtimeDreamMiddleware.create(
    ~adapter=CustomAdapter.create(),
  );
```

## Related Documentation

- [universal-reason-react/store](universal-reason-react.store.md) - Client-side store with sync
- [reason-realtime/pgnotify-adapter](reason-realtime.pgnotify-adapter.md) - PostgreSQL adapter
- [Dream Framework](https://github.com/aantron/dream) - Web framework documentation
