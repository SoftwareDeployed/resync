#!/usr/bin/env bash
set -euo pipefail

psql_cmd=(psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB")

"${psql_cmd[@]}" <<'SQL'
CREATE EXTENSION IF NOT EXISTS pgcrypto;

CREATE OR REPLACE FUNCTION uuidv7()
RETURNS uuid
LANGUAGE sql
AS $$
  SELECT gen_random_uuid();
$$;
SQL

for file in \
  /seed-sql/premise.sql \
  /seed-sql/period.sql \
  /seed-sql/inventory.sql \
  /seed-sql/cart.sql \
  /seed-sql/generated/realtime.sql
do
  if [[ -s "$file" ]]; then
    echo "Applying $file"
    "${psql_cmd[@]}" -f "$file"
  fi
done
