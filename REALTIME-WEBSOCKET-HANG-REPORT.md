# Realtime WebSocket Hang Investigation

## Summary

SurgStack browser tests reproduced a realtime server hang after a WebSocket
connection closed while another connection was still loading query snapshots.
The process stopped responding to `/readyz` and `gateway.exe` spun at high CPU.

The last server log before the hang was consistently an unmatched:

```text
send.begin
```

No `send.sent`, `send.timeout`, or `send.error` followed. The configured
`Lwt.pick` timeout did not fire because the scheduler was not getting control.

## Evidence

- Browser test: `loads dashboards through realtime queries without REST calls`.
- Realtime process: `gateway.exe` at roughly 87-95% CPU after the hang.
- `/readyz` requests accumulated and did not complete.
- Log parser found one unmatched `send.begin`. The first reproduction involved
  a large snapshot, but a later reproduction hung on an 843 byte scorecard
  snapshot, proving the 4096 byte Dream flush path was not the full cause.
- Dream source inspection showed `Dream.send` delegates to
  `Dream_pure.Message.send`, which calls `Stream.write`.
- Upstream Dream issue
  <https://github.com/camlworks/dream/issues/411> reports the same closed
  websocket behavior: repeated `writev` returning `EPIPE`, high CPU, and hot
  frames in `Faraday_lwt_unix`, `Gluten_lwt.write_loop_step`, and
  `Httpun_ws.Wsd.next`.
- Upstream Gluten issue <https://github.com/anmonteiro/gluten/issues/85>
  identifies the lower-level bug: the Lwt write loop reports `` `Closed`` and
  then recurses instead of exiting.
- Older Dream issue <https://github.com/camlworks/dream/issues/230> reports the
  same symptom after closing a browser tab and then calling `Dream.send`.

## Root Cause

Executor's realtime middleware used `Dream.send` under the assumption that an
Lwt timeout around the promise could bound write latency. That assumption is
wrong for this Dream/httpaf path.

The actual lower-level lockup is in Gluten's Lwt write loop. When a websocket
write hits a closed peer, `Faraday_lwt_unix.writev_of_fd` returns `` `Closed``.
`Gluten_lwt.write_loop_step` reported the closed write result to the protocol
and then immediately recursed. The websocket writer continued to expose pending
or closing output, so the loop could spin without yielding and starve the whole
Dream server.

The earlier Dream/httpaf flush patch reduced one synchronous risk but did not
fix the reproducible hang. The decisive evidence was a later browser run where
the server hung after `conn=6 send.begin len=843`, below the 4096 byte flush
threshold.

## Fix Applied

Executor now vendors the locked Gluten 0.5.2 source as
`vendor/gluten-no-closed-write-loop` with one targeted patch:

- in `lwt/gluten_lwt.ml`, after `writev` returns `` `Closed``, report the
  result, wake the write-loop-exited promise, and stop recursing

Executor also keeps the earlier Dream alpha vendor at
`vendor/dream-httpaf-no-blocking-flush`:

- keep Dream's WebSocket close handling intact
- do not call `Httpun_ws.Wsd.flushed` from the WebSocket outgoing loop
- immediately continue the outgoing loop after resetting the local byte counter

SurgStack bootstrap pins `gluten`, `gluten-lwt`, `gluten-lwt-unix`, and
`dream-httpaf` to executor-vendored paths before pinning `resync`, so Docker
builds use the patched dependencies.

## DX Follow-Ups

- Add an executor integration test that opens two WebSocket clients, closes one,
  and sends a large snapshot on the other.
- Expose a realtime server health metric for unmatched send attempts.
- Avoid treating `Lwt.pick` as a hard timeout around code paths that can spin
  synchronously before yielding.

## Related Finding: No-Cache Mutation Ack Timeout

SurgStack later reproduced a different failure: a `run_report` mutation rendered
completed report rows from PostgreSQL, but the UI still showed `Timed out
waiting for acknowledgement`.

Browser-side Playwright frame capture isolated the test page from manual
browser activity. The page used one realtime WebSocket and received a batch
containing the `run_report` ack plus report run/result/artifact snapshots. The
snapshots rendered correctly, proving the server, PostgreSQL work, and
WebSocket delivery path had completed.

The remaining failure was in the client store runtime:

- `StoreActionLedger.ackTimeoutMs` is 5000 ms.
- SurgStack's synced store uses `cache: None`.
- `StoreOffline.handleAckTimeout` read the pending action only through the
  cache adapter.
- With no cache, `cacheGetAction` always returned `None`, so the first
  5-second timeout became terminal and rejected the mutation promise.
- A valid server ack that arrived later could update query snapshots, but it
  could not reverse the already rejected mutation promise.

Executor now keeps an in-memory pending action ledger inside
`StoreOffline.Synced`. Persistent cache remains a durability layer, not the only
source of pending mutation truth. Timeout handling and ack handling read the
in-memory ledger first, then fall back to persistent cache. Settled action IDs
are remembered briefly so duplicate late acks are ignored for no-cache stores.

DX follow-up: add a package-level browser test for a no-cache synced store where
a valid ack arrives after the first ack-timeout window but before final retry
exhaustion.

## Related Finding: Stale Subscribers After Send Failure

The repeated SurgStack report workflow exposed another middleware bug after the
no-cache timeout fix. Browser frame capture showed valid mutation frames being
sent, but later mutations were delayed while the realtime server broadcasted to
many old connection IDs.

Realtime logs showed broadcast fan-out to stale subscribers whose writers were
already closed:

- `send.error ... Failure("cannot write to closed writer")`
- `send.timeout elapsed=2.0 ...`
- the same closed connection IDs remained in channel subscriber lists and were
  retried on later broadcasts

The cleanup path existed for `Dream.receive = None`, but `send_with_timeout`
swallowed send errors and timeouts as `unit`. The send queue therefore had no
signal that the subscriber should be detached.

Executor now returns a send outcome from `send_with_timeout`. Subscriber send
queues detach their websocket from channel lists on timeout or send error, drop
queued messages, and mark the connection as closed. Detach is idempotent by
connection ID so a later receive-close event cannot double-decrement connection
counters.

DX follow-up: add an integration test that leaves a closed websocket in a
subscriber list, broadcasts to the channel, and asserts the subscriber is
removed before the next broadcast.

## Related Finding: Queued WebSocket Frames Waiting For Ping Or Close

SurgStack later reproduced a separate intermittent hang where the server did
not spin, but already-sent client frames were not delivered to middleware until
another frame, ping, or close event woke the socket.

The isolated reproduction did not mount the React app. A Playwright page opened
one direct WebSocket to `/_events_v12`, sent three `select` frames, then sent a
burst containing `unsubscribe`, multiple `select` frames, and a `create_report`
mutation. Before the fix, realtime logs showed the first one or two frames in a
burst being handled immediately, then a later frame appearing roughly 15
seconds later when the probe timed out or closed the socket. The delayed frame
had not reached the middleware, SQL pool, mutation dispatcher, or send queue.

The root cause was in `httpun-ws`:

- `Websocket_connection` keeps a `frame_queue` for parsed frames.
- When the current payload became `Complete`, `_next_read_operation` only
  advanced the queue for selected parser states.
- A completed payload could therefore leave another already-parsed frame queued
  internally while the runtime yielded and waited for a future socket read.
- A ping, close, or new client frame eventually woke the parser, which made the
  stale queued frame appear much later.

Executor now vendors `httpun-ws` 0.2.0 at
`vendor/httpun-ws-drain-frame-queue`. The patch changes
`lib/websocket_connection.ml` so a completed payload advances the frame queue
and recurses before yielding, while preserving parser-error handling.

Regression coverage was added in the vendored package:

- `completed payload advances queued frame` parses two frames from one socket
  read, drains only the first payload, calls `next_read_operation`, and asserts
  the second queued frame is handled without another socket read.

Verification on June 14, 2026:

- `opam exec -- dune exec --root vendor/httpun-ws-drain-frame-queue ./lib_test/test_httpun_ws.exe`
- `opam exec -- dune build --root vendor/httpun-ws-drain-frame-queue httpun-ws.install httpun-ws-lwt.install httpun-ws-lwt-unix.install @runtest --display=short`
- `opam exec -- dune build packages/reason-realtime/dream-middleware packages/universal-reason-react/store/js --display=short`
- SurgStack containers pinned executor commit `caa30a085343a4bcd88e6cdf031e9b2e43c2c2f2` and `httpun-ws` to `/home/opam/executor-full-stack-pin/vendor/httpun-ws-drain-frame-queue`.
- Raw direct-WebSocket probe `raw-1781473908948` received initial
  `enterprise-demo`, `reports-demo`, and `commissions-demo` snapshots in 25 ms.
  The second mutation ack arrived 58 ms after the unsubscribe/select/mutation
  burst.
- `docker compose run --rm browser-tests pnpm exec playwright test tests/browser/smoke.spec.ts -g "creates, runs, charts, and exports reports repeatedly"` passed in both Chromium and mobile Chrome. Each project ran the 10-iteration create/run/chart/export workflow.

DX follow-up: SurgStack bootstrap currently performs multiple reinstall passes
after an executor HEAD change: `httpun-ws`, then `gluten`, then `gluten-lwt`
and downstream packages, then `dream-httpaf/dream`, then `resync`. This
rebuilds `resync` several times and makes dependency debugging slow. Collapse
the bootstrap into one dependency reinstall plan after all pins are applied.
