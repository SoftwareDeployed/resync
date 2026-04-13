DROP TABLE IF EXISTS inventory_period_map;
DROP TABLE IF EXISTS inventory;

-- @table inventory
-- @id_column id
-- @broadcast_channel column=premise_id
CREATE TABLE inventory (
  id uuid not null default uuidv7() primary key,
  premise_id uuid not null,
  name varchar not null,
  description varchar not null,
  quantity int not null default 0,
  FOREIGN KEY (premise_id) REFERENCES premise(id)
);

/*
@query get_complete_inventory
@cache_key inventory_id
@json_column period_list
SELECT
  i.description,
  i.id,
  i.name,
  i.quantity,
  i.premise_id,
  COALESCE(
    JSONB_AGG(
      TO_JSONB(p.*)
    ) FILTER (WHERE p.id IS NOT NULL),
    '[]'::jsonb
  )::text as period_list
FROM inventory i
JOIN inventory_period_map pm ON pm.inventory_id = i.id
JOIN period p ON p.id = pm.period_id
WHERE i.id = $1
GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;
*/

/*
@query get_inventory_list
@json_column period_list
SELECT
  i.description,
  i.id,
  i.name,
  i.quantity,
  i.premise_id,
  COALESCE(
    JSONB_AGG(
      TO_JSONB(p.*)
    ) FILTER (WHERE p.id IS NOT NULL),
    '[]'::jsonb
  )::text as period_list
FROM inventory i
JOIN inventory_period_map pm ON pm.inventory_id = i.id
JOIN period p ON p.id = pm.period_id
WHERE i.premise_id = $1
GROUP BY i.id, i.premise_id, i.name, i.description, i.quantity;
*/

INSERT INTO inventory (id, premise_id, name, description, quantity)
VALUES (
  'b55351b1-1b78-4b6c-bd13-6859dc9ad411',
  'a55351b1-1b78-4b6c-bd13-6859dc9ad410',
  'Pop Item',
  'An incredibly stinky product',
  1
);

-- @table inventory_period_map
-- @composite_key inventory_id, period_id
-- @broadcast_parent table=inventory query=get_complete_inventory
CREATE TABLE inventory_period_map (
  inventory_id uuid not null,
  period_id uuid not null,
  FOREIGN KEY (inventory_id) REFERENCES inventory(id),
  FOREIGN KEY (period_id) REFERENCES period(id),
  PRIMARY KEY (inventory_id, period_id)
);

INSERT INTO inventory_period_map (inventory_id, period_id)
SELECT 'b55351b1-1b78-4b6c-bd13-6859dc9ad411', id
FROM period;

/*
@mutation update_quantity
UPDATE inventory SET quantity = $2 WHERE id = $1;
*/
