# Realtime SQL Pool and Channel Safety Report

## Summary

SurgStack exposed two realtime failure modes that executor should make harder to create:

1. Websocket query callbacks were allowed to use `Dream.sql request` from long-lived realtime request handlers. Dream documents and implements a re-entrant SQL guard because nested or request-bound pool usage can deadlock or wedge request handling.
2. Database-backed realtime channels were dynamically built from user-facing slugs without an executor-side guardrail for PostgreSQL `LISTEN` channel length. PostgreSQL notification channel names are limited to 63 bytes; a `report-artifacts-` prefix plus a 47-character report slug produced a 64-character channel and raised `Failure("Invalid PostgreSQL channel name: exceeds maximum length (63)")`.

The application-level mitigation now uses an explicit Caqti pool for realtime snapshots/mutations and caps generated report slugs against the longest realtime channel prefix. The executor DX should still prevent this class of issue at the framework boundary.

## Evidence

- Dream's SQL helper stores an `acquired_sql_connection` Lwt key and warns on re-entrant `Dream.sql` calls because they can deadlock.
- Executor `reason-realtime/dream-middleware` defaults `use_db` to `Dream.sql`.
- Executor `server_builder.ml` wraps the whole app in `Dream.sql_pool` when `db_uri` is set.
- SurgStack originally resolved subscriptions and loaded snapshots with `Dream.sql request` inside websocket callbacks.
- Browser behavior alternated between working mutation acks and stuck query updates because mutation and query paths were sharing a fragile request-bound realtime/SQL execution context.
- Realtime logs showed successful mutation receipt followed by missing/stalled query refreshes until the app stopped relying on DB notification patches alone and explicitly broadcast refreshed snapshots for changed report channels.
- A concrete channel safety bug was reproduced:
  - Generated slug: `playwright-revenue-review-chromium-178-b1df4904` length 47.
  - Generated channel: `report-artifacts-playwright-revenue-review-chromium-178-b1df4904` length 64.
  - Executor adapter raised: `Invalid PostgreSQL channel name: exceeds maximum length (63)`.

## Application Fixes Applied in SurgStack

- Realtime runtime now creates its own Caqti pool and passes `~use_db` to middleware instead of using `Dream.sql request` for websocket DB work.
- Realtime gateway no longer wraps the websocket server in `Dream.sql_pool`.
- Successful report mutations now explicitly broadcast refreshed snapshots for affected channels:
  - `reports-demo`
  - `kpis-demo`
  - `report-runs-{slug}`
  - `report-results-{slug}`
  - `report-artifacts-{slug}`
- Report slug generation now caps the slug at `63 - String.length "report-artifacts-"`, keeping the longest generated PostgreSQL notification channel valid.
- Targeted browser verification passed for desktop Chromium and mobile Chrome: create report, reactive report-list update, open detail, run report, render result table/chart, and export CSV.

## Executor DX Recommendations

1. Add a first-class realtime database pool abstraction.
   - Middleware should accept DB-backed `resolve_subscription` and `load_snapshot` callbacks that receive a checked-out Caqti connection.
   - App authors should not need to pass `Dream.request` into long-lived websocket DB callbacks.

2. Make `Dream.sql` usage in websocket callbacks visibly unsafe.
   - Document this in `reason-realtime/dream-middleware`.
   - Add a runtime warning when middleware is created with default `use_db = Dream.sql` and DB-backed snapshots/resolvers are used.
   - Prefer an executor helper that builds a realtime-safe pool and passes `~use_db`.

3. Add channel-name validation utilities before adapter subscription.
   - Provide `Channel.make : prefix:string -> id:string -> (string, error) result`.
   - Include the PostgreSQL 63-byte limit in the helper.
   - Return a structured client-visible error instead of raising from `Adapter.subscribe`.

4. Add contract tests that cover mutation plus query refresh together.
   - Subscribe to a query.
   - Send a mutation that changes that query.
   - Assert the mutation ack resolves and the subscribed query receives a fresh snapshot without a page reload.

5. Add a `useQuery` skip-transition regression test.
   - Mount a query with `skip=true`.
   - Flip to `skip=false`.
   - Assert the realtime `select` frame is sent.
   - SurgStack still has symptoms around later dashboard queries not initializing after gating, so this should be tested at the executor hook level.

6. Preserve ping/pong health checks.
   - Ping/pong is still present and useful.
   - Health checks should not mask cases where the socket is alive but a subscribed query channel is no longer receiving snapshots.

## Suggested Executor Tests

- Middleware unit test: adapter subscription errors are converted to a websocket error frame and do not leave a half-registered in-memory channel.
- Adapter unit test: overlong channel names return structured errors.
- Browser/runtime test: route navigation from `/reports` to dashboard initializes new `useQuery` subscriptions and resolves loading states without a full document reload.
- Browser/runtime test: report-style mutation updates a subscribed list and a detail query in the same session.

