DROP TABLE IF EXISTS period;
-- The different units that can be used.
CREATE TYPE unit_enum AS ENUM ('second', 'minute', 'hour', 'day', 'week', 'month', 'year');

-- A period creates the unit of time that an inventory may be rented for
-- We can set the minimum and maximum period, price per period type...
-- These periods can be named and used with many different inventories.
CREATE TABLE period (
    id uuid not null DEFAULT uuidv7() primary key,
    label varchar not null,
    -- XXX: Should min/max_value be in a different table?
    min_value int not null,
    max_value int not null,
    unit unit_enum not null,
    -- This is the price in cents (per period / unit)
    price bigint not null
);

-- Example period Monthly with min/max of 1 month $100/month
INSERT INTO period (label, min_value, max_value, unit, price) VALUES ('Monthly', 1, 1, 'month', 100000);
INSERT INTO period (label, min_value, max_value, unit, price) VALUES ('Hourly', 1, 12, 'hour', 10000);
