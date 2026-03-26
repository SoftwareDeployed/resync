DROP TABLE IF EXISTS inventory_period_map;
DROP TABLE IF EXISTS inventory;

CREATE TABLE inventory (
  id uuid not null default uuidv7() primary key,
  premise_id uuid not null,
  name varchar not null,
  description varchar not null,
  quantity int not null default 0,
  FOREIGN KEY (premise_id) REFERENCES premise(id)
);

INSERT INTO inventory (id, premise_id, name, description, quantity)
VALUES (
  'b55351b1-1b78-4b6c-bd13-6859dc9ad411',
  'a55351b1-1b78-4b6c-bd13-6859dc9ad410',
  'Pop Item',
  'An incredibly stinky product',
  1
);

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
