# reason-realtime/dream-middleware

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.

Dream websocket middleware and adapter hook for real-time server-to-client updates.

## Overview

This package provides a minimal websocket transport for pushing messages from server-side data sources into connected Dream clients.

The middleware owns websocket lifecycle and channel subscriptions; it delegates two important decisions to your app:

- how a client selection string maps to a channel (`~resolve_subscription`)
- how to load a channel snapshot (`~load_snapshot`)

## Features

- WebSocket endpoint helper for Dream
- Request-aware channel resolution via callback
- Snapshot loading on subscription
- Adapter integration for backend message sources (PostgreSQL adapter, custom adapters)
- Broadcast fan-out to all sockets subscribed to a channel

## Installation

Add to your `dune` file:

```lisp
(libraries
  reason_realtime_dream_middleware
  reason_realtime_pgnotify_adapter
  dream
  lwt)
```

## Quick Start

### Basic Setup

```reason
// server.ml
open Dream;

let resolve_subscription request selection = {
  let route = Dream.path request in
  let user = switch (Dream.session_field(request, "user_id")) {
  | Some(value) => value
  | None => "anonymous"
  };
  if (String.equal(selection, "public")) {
    Lwt.return(Some("inventory"));
  } else {
    Lwt.return(Some(user ++ ":" ++ selection));
  };
};

let load_snapshot request channel = {
  let body =
    if (String.equal(channel, "inventory")) {
      "{\"type\":\"snapshot\",\"channel\":\"inventory\",\"payload\":{}}"
    } else {
      "{\"type\":\"snapshot\",\"channel\":\"\" ++ channel,\"payload\":{}}"
    }
  in
  Lwt.return(body);
};

let pg_adapter = Pgnotify_adapter.create(~db_uri="postgres://user:pass@localhost:5432/mydb", ());

let adapter = Adapter.pack(pg_adapter);

let middleware =
  Middleware.create(
    ~adapter,
    ~resolve_subscription,
    ~load_snapshot,
  );

let () =
  (* Start adapter polling *)
  let _ = Lwt.async(() => Adapter.start(adapter));
  Dream.run
  @@ Dream.logger
  @@ Dream.router([
    Dream.get "/_events" (Middleware.route "/_events" middleware),
    Dream.get "/**" (fun request =>
      Dream.html("Hello from app")
    ),
  ]);
```

### Broadcasting from Adapter Callbacks

The adapter should call the middleware broadcast function with string payloads when source events arrive.

```reason
let middleware =
  Middleware.create(
    ~adapter,
    ~resolve_subscription,
    ~load_snapshot,
  );

let () =
  let* () = Middleware.broadcast(middleware, "inventory", "{\"type\":\"patch\"}") in
  Lwt.return_unit;
```

## API Reference

### Types

```reason
type t;
```

### Functions

#### `Middleware.create`

```reason
let create: (
  ~adapter: Adapter.packed,
  ~resolve_subscription: (Dream.request => string => string option Lwt.t),
  ~load_snapshot: (Dream.request => string => string Lwt.t),
) => Middleware.t;
```

Build middleware and provide callbacks for subscription and snapshot resolution.

#### `Middleware.route`

```reason
let route: (string, Middleware.t) => Dream.handler;
```

Create a Dream handler for a websocket endpoint path and middleware.

#### `Middleware.broadcast`

```reason
let broadcast: (Middleware.t, string, string) => Lwt.t(unit);
```

Broadcast a payload to all connected clients subscribed to the channel.

#### `Adapter`

The `Adapter` module defines the adapter protocol:

```reason
module type S = sig
  type t
  val start : t -> unit Lwt.t
  val stop : t -> unit Lwt.t
  val subscribe : t -> channel:string -> handler:(string -> unit Lwt.t) -> unit Lwt.t
  val unsubscribe : t -> channel:string -> unit Lwt.t
end

type packed = Pack : (module S with type t = 'a) * 'a -> packed

let pack : (module S with type t = 'a) -> 'a -> packed
let start : packed -> unit Lwt.t
let stop : packed -> unit Lwt.t
let subscribe : packed -> channel:string -> handler:(string -> unit Lwt.t) -> unit Lwt.t
let unsubscribe : packed -> channel:string -> unit Lwt.t
```

## Message Protocol

Clients send plain text commands over websocket.

Supported commands:

- `ping` → server replies with `pong`
- `select <channel>` → subscribe/replace active subscription

Responses are plain text payloads sent by the server (for example snapshots and adapter messages).

## Troubleshooting

### Clients Can't Connect

1. Confirm Dream routes include your websocket path using `Middleware.route`.
2. Verify no proxy strips websocket upgrade headers.
3. Ensure the request path matches your client websocket URL.

### No Messages Received

1. Confirm adapter is running and emitting messages.
2. Confirm `resolve_subscription` returns the same channel the client selects.
3. Confirm `load_snapshot` returns a JSON string for that channel.

### Unexpected Subscriptions

All authorization and tenancy checks should happen in `resolve_subscription`/`load_snapshot`; this package does not currently provide policy helpers.

## Integration Notes

- Use `reason-realtime/pgnotify-adapter` for Postgres-backed event sources.
- For custom data sources, pass any adapter packed with `Adapter.pack`.
