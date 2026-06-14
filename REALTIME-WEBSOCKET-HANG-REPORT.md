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
- Log parser found one unmatched `send.begin`, commonly on a large snapshot.
- Dream source inspection showed `Dream.send` delegates to
  `Dream_pure.Message.send`, which calls `Stream.write`.
- Dream HTTP/WebSocket adapter calls `Httpun_ws.Wsd.flushed` synchronously when
  accumulated outgoing bytes cross 4096.

## Root Cause

Executor's realtime middleware used `Dream.send` under the assumption that an
Lwt timeout around the promise could bound write latency. That assumption is
wrong for this Dream/httpaf path.

When the WebSocket adapter enters `Httpun_ws.Wsd.flushed`, it can spin
synchronously during a close/write race. Since this happens before the Lwt
scheduler regains control, an `Lwt.pick` timeout cannot preempt it.

## Fix Applied

Executor now vendors the locked Dream alpha source as
`vendor/dream-httpaf-no-blocking-flush` with one targeted patch:

- keep Dream's WebSocket close handling intact
- do not call `Httpun_ws.Wsd.flushed` from the WebSocket outgoing loop
- immediately continue the outgoing loop after resetting the local byte counter

SurgStack bootstrap pins `dream-httpaf` to this executor-vendored path before
pinning `resync`, so Docker builds use the patched dependency.

## DX Follow-Ups

- Add an executor integration test that opens two WebSocket clients, closes one,
  and sends a large snapshot on the other.
- Expose a realtime server health metric for unmatched send attempts.
- Avoid treating `Lwt.pick` as a hard timeout around code paths that can spin
  synchronously before yielding.
