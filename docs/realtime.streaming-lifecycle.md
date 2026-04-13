# Realtime streaming lifecycle

This guide ties the stream package, the Dream middleware, and the store runtime together.

## End-to-end flow

1. A client dispatches an action.
2. The store updates optimistically and writes the action to its local ledger.
3. The client sends a `mutation` frame to the server.
4. The server runs the named SQL mutation or OCaml handler.
5. Postgres emits a row-change notification.
6. `reason-realtime/pgnotify-adapter` receives the notification.
7. `reason-realtime/dream-middleware` broadcasts a `patch`, `snapshot`, or `ack`.
8. The client receives the update and reconciles the store.

## Concrete frame examples

The llm-chat demo shows the shape of custom realtime events:

```reason
let stream_event_json ~event fields =
  Yojson.Basic.to_string
    (`Assoc [("type", `String "custom"); ("payload", `Assoc (("event", `String event) :: fields))]);
```

It uses that helper to broadcast `stream_started`, `token_received`, `stream_complete`, and `stream_error` events while the NDJSON stream is being read.

For SQL-triggered row changes, the generated payload looks like this:

```json
{"type":"patch","table":"inventory","id":"...","action":"UPDATE","data":{...}}
```

The important part is that the client only sees a stable patch frame shape; the source of the change can be SQL, a hand-written mutation handler, or a custom server event.

## Frame types

- `select` — subscription setup
- `mutation` — client write request
- `ack` — mutation acknowledgement
- `patch` — incremental row change
- `snapshot` — full state refresh

## Where the pieces live

- `docs/reason-realtime.stream.md` — stream parsing and Dream stream helpers
- `docs/reason-realtime.pgnotify-adapter.md` — Postgres LISTEN/NOTIFY bridge
- `docs/reason-realtime.dream-middleware.md` — websocket middleware and fan-out
- `docs/realtime-schema.sql-annotations.md` — SQL annotations that drive the generated triggers

## How the pieces fit in practice

- SQL annotations define which rows broadcast and how parent rows are re-queried.
- The generated triggers turn those annotations into `pg_notify` calls.
- `Pgnotify_adapter` receives the notification and hands it to middleware.
- Middleware fan-out then produces the websocket frames that the client store consumes.
- The store’s optimistic ledger fills the gap until the server ack arrives.

## Practical guidance

- Keep mutation handlers deterministic.
- Use generated query and mutation metadata instead of raw string concatenation.
- Use the stream package for token streams and line-delimited server responses, not for database change handling.
