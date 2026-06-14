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
