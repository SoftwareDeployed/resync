-- @table todo_lists
-- @id_column id
-- @broadcast_channel column=id

CREATE TABLE todo_lists (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  name text NOT NULL DEFAULT 'My Todo List',
  created_at timestamptz NOT NULL DEFAULT NOW(),
  updated_at timestamptz NOT NULL DEFAULT NOW()
);

CREATE TABLE processed_actions (
  id uuid PRIMARY KEY,
  created_at timestamptz NOT NULL DEFAULT NOW()
);

-- @table todos
-- @id_column id
-- @broadcast_channel column=list_id
-- @broadcast_parent table=todo_lists query=get_list

CREATE TABLE todos (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  list_id uuid NOT NULL REFERENCES todo_lists(id) ON DELETE CASCADE,
  text text NOT NULL,
  completed boolean NOT NULL DEFAULT false,
  created_at timestamptz NOT NULL DEFAULT NOW()
);

/*
@query get_list
SELECT id, list_id, text, completed FROM todos WHERE list_id = $1 ORDER BY created_at;
*/

/*
@mutation create_list
INSERT INTO todo_lists (id) VALUES ($1);
*/

/*
@mutation add_todo
WITH action_guard AS (
  INSERT INTO processed_actions (id)
  VALUES ($1::uuid)
  ON CONFLICT DO NOTHING
  RETURNING id
), inserted AS (
  INSERT INTO todos (id, list_id, text)
  SELECT $2::uuid, $3::uuid, $4 FROM action_guard
  RETURNING list_id
)
UPDATE todo_lists
SET updated_at = NOW()
WHERE id IN (SELECT list_id FROM inserted);
*/

/*
@mutation set_todo_completed
WITH action_guard AS (
  INSERT INTO processed_actions (id)
  VALUES ($1::uuid)
  ON CONFLICT DO NOTHING
  RETURNING id
), updated AS (
  UPDATE todos
  SET completed = $3
  WHERE id = $2::uuid AND EXISTS (SELECT 1 FROM action_guard)
  RETURNING list_id
)
UPDATE todo_lists
SET updated_at = NOW()
WHERE id IN (SELECT list_id FROM updated);
*/

/*
@mutation remove_todo
WITH action_guard AS (
  INSERT INTO processed_actions (id)
  VALUES ($1::uuid)
  ON CONFLICT DO NOTHING
  RETURNING id
), deleted AS (
  DELETE FROM todos
  WHERE id = $2::uuid AND EXISTS (SELECT 1 FROM action_guard)
  RETURNING list_id
)
UPDATE todo_lists
SET updated_at = NOW()
WHERE id IN (SELECT list_id FROM deleted);
*/

/*
@mutation rename_list
UPDATE todo_lists SET name = $2, updated_at = NOW() WHERE id = $1;
*/
