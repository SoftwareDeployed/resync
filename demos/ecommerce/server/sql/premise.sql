CREATE TABLE premise (
  id uuid primary key not null default uuidv7(),
  name varchar not null,
  description varchar not null,
  updated_at timestamp not null default now()
);

INSERT INTO premise (name, description) VALUES ('Example Premise', 'An example premise');

CREATE TABLE premise_route (
  premise_id uuid not null default uuidv7(),
  route_root varchar not null unique,
  FOREIGN KEY (premise_id) REFERENCES premise(id)
);

INSERT INTO premise_route VALUES ('a55351b1-1b78-4b6c-bd13-6859dc9ad410', '/');
