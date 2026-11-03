DROP TABLE IF EXISTS order_items CASCADE;
DROP TABLE IF EXISTS orders CASCADE;

-- @table orders
-- @id_column id
CREATE TABLE orders (
  id uuid not null default uuidv7() primary key,
  premise_id uuid not null,
  customer_name varchar not null,
  customer_email varchar not null,
  start_date varchar not null,
  end_date varchar not null,
  status varchar not null default 'confirmed',
  created_at timestamptz not null default now(),
  FOREIGN KEY (premise_id) REFERENCES premise(id)
);

-- @table order_items
-- @composite_key order_id, inventory_id
CREATE TABLE order_items (
  order_id uuid not null,
  inventory_id uuid not null,
  quantity int not null,
  unit_label varchar not null,
  unit_price_cents bigint not null,
  FOREIGN KEY (order_id) REFERENCES orders(id),
  FOREIGN KEY (inventory_id) REFERENCES inventory(id),
  PRIMARY KEY (order_id, inventory_id)
);

/*
@query get_order
SELECT
  o.id,
  o.premise_id,
  o.customer_name,
  o.customer_email,
  o.start_date,
  o.end_date,
  o.status,
  EXTRACT(EPOCH FROM o.created_at) AS created_at
FROM orders o
WHERE o.id = $1
LIMIT 1;
*/

/*
@query get_order_items
SELECT
  oi.inventory_id,
  i.name,
  oi.quantity,
  oi.unit_label,
  oi.unit_price_cents
FROM order_items oi
JOIN inventory i ON i.id = oi.inventory_id
WHERE oi.order_id = $1
ORDER BY i.name;
*/

/*
@mutation create_order
INSERT INTO orders (id, premise_id, customer_name, customer_email, start_date, end_date)
VALUES ($1, $2, $3, $4, $5, $6);
*/

/*
@mutation add_order_item
INSERT INTO order_items (order_id, inventory_id, quantity, unit_label, unit_price_cents)
VALUES ($1, $2, $3, $4, $5);
*/

/*
@mutation decrement_inventory
UPDATE inventory SET quantity = quantity - $2 WHERE id = $1;
*/
