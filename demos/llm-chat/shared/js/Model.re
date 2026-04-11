open Melange_json.Primitives;

module Message = {
  [@deriving json]
  type t = {
    id: string,
    thread_id: string,
    role: string,
    content: string,
  };
};

module Thread = {
  [@deriving json]
  type t = {
    id: string,
    title: string,
    updated_at: float,
  };
};

[@deriving json]
type t = {
  threads: array(Thread.t),
  current_thread_id: option(string),
  messages: array(Message.t),
  input: string,
  updated_at: float,
};
