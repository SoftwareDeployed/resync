#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${DB_URL:-}" ]]; then
  echo "DB_URL is required" >&2
  exit 1
fi

psql_cmd=(psql -v ON_ERROR_STOP=1 "$DB_URL")

"${psql_cmd[@]}" <<'SQL'
CREATE EXTENSION IF NOT EXISTS pgcrypto;

DROP TABLE IF EXISTS inventory_period_map CASCADE;
DROP TABLE IF EXISTS inventory CASCADE;
DROP TABLE IF EXISTS premise_route CASCADE;
DROP TABLE IF EXISTS premise CASCADE;
DROP TABLE IF EXISTS period CASCADE;
DROP TYPE IF EXISTS unit_enum CASCADE;
DROP FUNCTION IF EXISTS notify_row_change() CASCADE;

CREATE OR REPLACE FUNCTION uuidv7()
RETURNS uuid
LANGUAGE sql
AS $$
  SELECT gen_random_uuid();
$$;
SQL

for file in \
  demos/ecommerce/server/sql/premise.sql \
  demos/ecommerce/server/sql/period.sql \
  demos/ecommerce/server/sql/inventory.sql \
  demos/ecommerce/server/sql/cart.sql \
  demos/ecommerce/server/sql/generated/realtime.sql
do
  if [[ -s "$file" ]]; then
    echo "Applying $file"
    "${psql_cmd[@]}" -f "$file"
  fi
done
