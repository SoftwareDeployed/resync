type t = List(string);

let list = (list_id: string): t => List(list_id);

let prefix = "list";

let channel = (subscription: t): string =>
  switch (subscription) {
  | List(list_id) => list_id
  };

let encode = (subscription: t): string =>
  switch (subscription) {
  | List(list_id) => StoreChannel.encode(~prefix, ~id=list_id)
  };

let decode = (value: string): option(t) =>
  switch (StoreChannel.decodeId(~prefix, ~value)) {
  | Some(list_id) => Some(List(list_id))
  | None => None
  };

let decode_channel = (value: string): option(string) =>
  switch (decode(value)) {
  | Some(subscription) => Some(channel(subscription))
  | None => None
  };
