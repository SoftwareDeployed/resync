#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${DB_URL:-}" ]]; then
  echo "DB_URL is required" >&2
  exit 1
fi

psql_cmd=(psql -v ON_ERROR_STOP=1 "$DB_URL")

"${psql_cmd[@]}" <<'SQL'
CREATE EXTENSION IF NOT EXISTS pgcrypto;

DROP TABLE IF EXISTS messages CASCADE;
DROP TABLE IF EXISTS threads CASCADE;
DROP TABLE IF EXISTS processed_actions CASCADE;
SQL

for file in \
  demos/llm-chat/server/sql/schema.sql \
  demos/llm-chat/server/sql/generated/realtime.sql
do
  if [[ -s "$file" ]]; then
    echo "Applying $file"
    "${psql_cmd[@]}" -f "$file"
  fi
done
