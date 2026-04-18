#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${DB_URL:-}" ]]; then
  echo "DB_URL is required" >&2
  exit 1
fi

psql_cmd=(psql -v ON_ERROR_STOP=1 "$DB_URL")

"${psql_cmd[@]}" <<'SQL'
CREATE EXTENSION IF NOT EXISTS pgcrypto;

DROP TABLE IF EXISTS todos CASCADE;
DROP TABLE IF EXISTS todo_lists CASCADE;
DROP TABLE IF EXISTS processed_actions CASCADE;
SQL

for file in \
  demos/todo-multiplayer/server/sql/schema.sql \
  demos/todo-multiplayer/server/sql/generated/realtime.sql
do
  if [[ -s "$file" ]]; then
    echo "Applying $file"
    "${psql_cmd[@]}" -f "$file"
  fi
done

"${psql_cmd[@]}" <<'SQL'
CREATE TABLE IF NOT EXISTS _resync_actions_create_list (
  action_id uuid PRIMARY KEY,
  status text NOT NULL CHECK (status IN ('ok', 'failed')),
  processed_at timestamptz NOT NULL DEFAULT NOW(),
  error_message text
);

CREATE TABLE IF NOT EXISTS _resync_actions_add_todo (
  action_id uuid PRIMARY KEY,
  status text NOT NULL CHECK (status IN ('ok', 'failed')),
  processed_at timestamptz NOT NULL DEFAULT NOW(),
  error_message text
);

CREATE TABLE IF NOT EXISTS _resync_actions_set_todo_completed (
  action_id uuid PRIMARY KEY,
  status text NOT NULL CHECK (status IN ('ok', 'failed')),
  processed_at timestamptz NOT NULL DEFAULT NOW(),
  error_message text
);

CREATE TABLE IF NOT EXISTS _resync_actions_remove_todo (
  action_id uuid PRIMARY KEY,
  status text NOT NULL CHECK (status IN ('ok', 'failed')),
  processed_at timestamptz NOT NULL DEFAULT NOW(),
  error_message text
);

CREATE TABLE IF NOT EXISTS _resync_actions_rename_list (
  action_id uuid PRIMARY KEY,
  status text NOT NULL CHECK (status IN ('ok', 'failed')),
  processed_at timestamptz NOT NULL DEFAULT NOW(),
  error_message text
);

CREATE TABLE IF NOT EXISTS _resync_actions_fail_server_mutation (
  action_id uuid PRIMARY KEY,
  status text NOT NULL CHECK (status IN ('ok', 'failed')),
  processed_at timestamptz NOT NULL DEFAULT NOW(),
  error_message text
);

CREATE TABLE IF NOT EXISTS schema_migrations (
  version varchar PRIMARY KEY,
  applied_at timestamp NOT NULL DEFAULT NOW()
);
SQL
