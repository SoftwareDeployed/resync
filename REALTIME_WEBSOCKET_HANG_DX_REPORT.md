# Realtime WebSocket Hang DX Report

## Summary

SurgStack exposed an executor realtime failure that looked like a mutation/query bug from the UI, but the server-side symptom was a websocket send path that could stop making progress after subscription churn. The strongest reproduced failure was a `send.begin` log with no matching `send.sent`, `send.timeout`, or `send.error`, followed by the realtime service consuming a CPU core and no longer answering websocket traffic reliably.

The current mitigation is not claimed as a general proof that Dream/httpun can never wedge. It is supported by the specific SurgStack repro: a 10-iteration browser test that repeatedly creates, runs, charts, and exports reports in desktop Chromium and mobile Chrome, followed by log analysis showing all sends in that window completed and the realtime/web health checks still returned.

## What Failed

The application used one shared browser websocket for unrelated realtime query channels. During the report workflow, that socket carried:

- report list snapshots
- KPI snapshots
- report run snapshots
- report result snapshots
- report artifact snapshots
- mutation acknowledgements
- ping/pong health frames
- route-driven subscribe/unsubscribe churn

Before the latest executor change, instrumented logs showed cases where the server entered `Dream.send` for a websocket message and did not return. Because the call did not yield back to Lwt, the wrapper timeout around `Dream.send` did not fire either. That made the hang look like a broken mutation or stale query in the browser, while the server was actually stuck in the outgoing websocket path.

## Evidence Collected

Instrumentation added in executor:

- `reason-realtime/dream-middleware`
  - connection open/close lifecycle
  - receive loop timing
  - subscribe begin/snapshot/adapter/cleanup phases
  - mutation ack send phases
  - queued websocket send begin/completion/error/timeout
- `reason-realtime/pgnotify-adapter`
  - listen/notify connection lifecycle
  - wait-for-input loop timing
  - channel subscription errors
- `reason-realtime/dream-middleware/sql_action_store`
  - action ledger and mutation handler SQL phases

Earlier failing traces showed:

- a websocket `send.begin` with no matching completion
- realtime CPU near a full core after the stuck send
- no corresponding PostgreSQL lock or channel-name error in that failing window
- ping/pong frames could also stop completing once the websocket send path was wedged

Fresh verification after the per-channel websocket change:

```sh
cd /Users/briankaplan/dev-current/fucundo/surgstack-next
date -u +%Y-%m-%dT%H:%M:%SZ > /tmp/surgstack-evidence-start
docker compose run --rm browser-tests pnpm exec playwright test tests/browser/smoke.spec.ts -g "creates, runs, charts, and exports reports repeatedly"
docker compose logs --since "$(cat /tmp/surgstack-evidence-start)" realtime > /tmp/surgstack-evidence-realtime.log
```

Result:

- Chromium report stress test: passed
- mobile Chrome report stress test: passed
- realtime `/readyz`: returned ok after the test
- web `/readyz`: returned ok after the test
- realtime container CPU after the test: low, not pegged
- send log analysis for the test window:
  - `unmatched_total=0`
  - `send.timeout=0`
  - `send.error=0`
  - `max_send_elapsed=0.002`

The only `timeout` lines in the fresh log window were `pgnotify wait_for_input.timeout`, which are the normal idle LISTEN polling timeout logs, not websocket send timeouts.

## Fixes Applied In Executor

The relevant executor commits are:

- `d0c9060 Trace realtime websocket lifecycle`
- `8c01084 Drain websocket sends in caller flow`
- `3f24f63 Avoid duplicate selects for active websocket channels`
- `29cb860 Debounce query resync after realtime patches`
- `9f40ee6 Scope realtime websockets by channel`

The change that made the SurgStack 10-iteration browser repro pass was scoping browser websocket state by realtime channel instead of by event URL only. Observers of the same channel still share a socket, but unrelated channels no longer pile all snapshots, patches, pings, route churn, and mutation traffic onto one Dream websocket.

Two supporting changes are still useful:

- Duplicate observer subscriptions no longer send duplicate `select` frames for a channel that is already active on the same websocket.
- Query patch handling now debounces the follow-up resync `select`, reducing immediate burst traffic after database notifications.

## SQL Pool Finding

The user's concern about `Dream.sql_pool` usage is valid as a DX issue. `Dream.sql` is request-bound, and it is unsafe for executor users to accidentally carry request-bound SQL access into long-lived websocket callbacks or detached work. Executor should make that hard or impossible.

For this later intermittent hang, the instrumented failing traces did not support SQL pool re-entry as the immediate cause:

- mutation SQL phases completed in the logged cases
- action ledger phases completed or failed visibly
- pgnotify LISTEN activity continued until the websocket send path stopped progressing
- the stuck point observed in the strongest trace was after `send.begin`

So this report treats SQL-pool misuse as a real framework footgun, but not the proven proximate cause of the final reproduced websocket hang.

## Recommended Executor DX Work

1. Add a browser/runtime regression that opens several independent realtime query channels, sends payloads above the websocket flush threshold, then rapidly closes and reconnects clients. Assert the server still answers `/readyz`, pings complete, and every logged send either completes or reports a timeout/error.

2. Keep structured websocket lifecycle logging available behind an environment flag. The key diagnostic is a unique send id with begin/completion/error/timeout, connection id, channel id, payload length, and elapsed time.

3. Add a transport-level watchdog for sends that start but do not complete. A normal Lwt timeout is not enough if the lower-level send call does not yield.

4. Expose websocket multiplexing policy as an explicit executor configuration. The default should favor isolation by channel unless executor has a proven safe multiplexing strategy for high-churn, high-payload apps.

5. Add executor tests for duplicate observers and patch resync behavior:
   - two observers for one query should not emit duplicate initial selects
   - a patch should schedule one bounded resync, not an unbounded burst
   - unmount should cancel pending resync work

6. Add a first-class realtime database pool abstraction. Middleware callbacks that need SQL should receive a safe checked-out connection instead of reaching for `Dream.sql request` from long-lived websocket logic.

7. Preserve ping/pong, but report it separately from query health. Ping/pong proves the transport can answer small health frames; it does not prove subscribed query channels are still receiving snapshots.

## Remaining Risk

The current SurgStack repro no longer hangs, but executor should still get a smaller isolated reproduction against Dream/httpun. The important unanswered question is why one `Dream.send` call could fail to yield strongly enough that the surrounding Lwt timeout did not run. Until executor has an isolated transport-level test for that condition, per-channel scoping is an evidence-backed mitigation for SurgStack, not a complete upstream root-cause proof.
