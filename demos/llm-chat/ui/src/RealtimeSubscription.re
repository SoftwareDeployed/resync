type t =
  | Thread(string);

let thread = (thread_id: string): t => Thread(thread_id);

let prefix = "thread";

let channel = (subscription: t): string =>
  switch (subscription) {
  | Thread(thread_id) => thread_id
  };

let encode = (subscription: t): string =>
  switch (subscription) {
  | Thread(thread_id) => StoreChannel.encode(~prefix, ~id=thread_id)
  };

let decode = (value: string): option(t) =>
  switch (StoreChannel.decodeId(~prefix, ~value)) {
  | Some(thread_id) => Some(Thread(thread_id))
  | None => None
  };

let decode_channel = (value: string): option(string) =>
  switch (decode(value)) {
  | Some(subscription) => Some(channel(subscription))
  | None => None
  };
