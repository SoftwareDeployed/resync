-- @table threads
-- @id_column id
-- @broadcast_channel column=id
-- @broadcast_to_views table=active_thread_views channel=thread_id
CREATE TABLE threads (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  title text NOT NULL DEFAULT 'New Chat',
  created_at timestamptz NOT NULL DEFAULT NOW(),
  updated_at timestamptz NOT NULL DEFAULT NOW()
);

CREATE TABLE processed_actions (
  id uuid PRIMARY KEY,
  created_at timestamptz NOT NULL DEFAULT NOW()
);

-- @table messages
-- @id_column id
-- @broadcast_channel column=thread_id
-- @broadcast_parent table=threads query=get_thread
CREATE TABLE messages (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  thread_id uuid NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
  role text NOT NULL,
  content text NOT NULL DEFAULT '',
  created_at timestamptz NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS active_thread_views (
  thread_id uuid REFERENCES threads(id) ON DELETE CASCADE,
  PRIMARY KEY (thread_id)
);

/*
@query get_thread
SELECT id, title, EXTRACT(EPOCH FROM created_at) * 1000 AS created_at, EXTRACT(EPOCH FROM updated_at) * 1000 AS updated_at FROM threads WHERE id = $1;
*/

/*
@query get_messages
SELECT id, thread_id, role, content, EXTRACT(EPOCH FROM created_at) * 1000 AS created_at FROM messages WHERE thread_id = $1 ORDER BY created_at;
*/

/*
@mutation create_thread
INSERT INTO threads (id, title) VALUES ($1::uuid, $2);
*/

/*
@mutation add_message
WITH action_guard AS (
  INSERT INTO processed_actions (id)
  VALUES ($1::uuid)
  ON CONFLICT DO NOTHING
  RETURNING id
)
INSERT INTO messages (id, thread_id, role, content)
SELECT $2::uuid, $3::uuid, $4, $5 FROM action_guard;
*/

/*
@mutation update_message_content
UPDATE messages SET content = $2 WHERE id = $1::uuid;
*/
