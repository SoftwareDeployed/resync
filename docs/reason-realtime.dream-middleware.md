# reason-realtime/dream-middleware

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.

Dream websocket middleware and adapter hook for realtime server-to-client updates and client-to-server JSON action frames.

## Overview

This package provides a minimal websocket transport for pushing messages from server-side data sources into connected Dream clients and for accepting JSON mutation frames from connected browsers.

The middleware owns websocket lifecycle and channel subscriptions; it delegates three important decisions to your app:

- how a client selection string maps to a channel (`~resolve_subscription`)
- how to load a channel snapshot (`~load_snapshot`)
- how to execute a typed action frame (`~handle_mutation`, optional)

## Features

- WebSocket endpoint helper for Dream
- Request-aware channel resolution via callback
- Snapshot loading on subscription
- Adapter integration for backend message sources (PostgreSQL adapter, custom adapters)
- Broadcast fan-out to all sockets subscribed to a channel
- Optional action callback for frames like `{type: "mutation", actionId, action}`

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
    (),
  );

let () =
  (* Start adapter polling *)
  let _ = Lwt.async(() => Adapter.start(adapter));
  Dream.run
  @@ Dream.logger
  @@ Dream.router([
    Middleware.route "/_events" middleware,
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
    (),
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
  ?handle_mutation: (Dream.request => action_id:string => Yojson.Basic.t => (unit, string) result Lwt.t),
  unit,
) => Middleware.t;
```

Build middleware and provide callbacks for subscription resolution, snapshot loading, and optional action execution.

#### `Middleware.route`

```reason
let route: (string, Middleware.t) => Dream.route;
```

Create the websocket route for `Dream.router`.

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

Clients send JSON messages over websocket.

Supported messages:

- `ping` → server replies with `pong`
- `{type: "select", subscription, updatedAt}` → subscribe or replace the active subscription
- `{type: "mutation", actionId, action}` → invoke `~handle_mutation`

Responses are JSON payloads sent by the server, including `snapshot`, `patch`, `ack`, and `pong`.

### Mutation Example

```reason
let handle_mutation request ~action_id action = {
  let kind = (* decode action.kind from Yojson.Basic.t *) "add_todo";
  switch (kind) {
  | "add_todo" => {
      let payload = (* decode action.payload *) action;
      let listId = "...";
      let text = "...";
      let* () = Dream.sql(request, Database.Todo.add_todo((action_id, listId, text))) in
      Lwt.return(Ok(()));
    }
  | _ => Lwt.return(Error("Unknown action kind"))
  };

let middleware =
  Middleware.create(
    ~adapter,
    ~resolve_subscription,
    ~load_snapshot,
    ~handle_mutation,
    (),
  );
```

## Troubleshooting

### Clients Can't Connect

1. Confirm Dream routes include your websocket path using `Middleware.route`.
2. Verify no proxy strips websocket upgrade headers.
3. Ensure the request path matches your client websocket URL.

### No Messages Received

1. Confirm adapter is running and emitting messages.
2. Confirm `resolve_subscription` returns the same channel the client selects.
3. Confirm `load_snapshot` returns a JSON string for that channel.
4. Confirm your action handler executes database writes that trigger broadcasts on the selected channel.

### Unexpected Subscriptions

All authorization and tenancy checks should happen in `resolve_subscription`/`load_snapshot`; this package does not currently provide policy helpers.

## Integration Notes

- Use `reason-realtime/pgnotify-adapter` for Postgres-backed event sources.
- For custom data sources, pass any adapter packed with `Adapter.pack`.

## Testing

Native protocol/lifecycle coverage now exists for this package.

Build:

```bash
opam exec -- dune build @dream-middleware-tests
```

Run:

```bash
opam exec -- dune exec ./packages/reason-realtime/dream-middleware/test/dream_middleware_test.exe
```

Current native test cases in `packages/reason-realtime/dream-middleware/test/middleware_behavior_test.ml`:

- `ping replies with pong`
- `select subscribes and sends snapshot`
- `mutation success sends ack ok`
- `invalid mutation sends error ack`
- `media handler error sends error frame`
- `detach unsubscribes active channel`

For the cross-package test inventory and current coverage notes, see [testing.md](testing.md).
