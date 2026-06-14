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
