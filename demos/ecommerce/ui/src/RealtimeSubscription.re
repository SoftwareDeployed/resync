type t =
  | Premise(string);

let premise = (premise_id: string): t => Premise(premise_id);

let prefix = "premise";

let channel = (subscription: t): string =>
  switch (subscription) {
  | Premise(premise_id) => premise_id
  };

let encode = (subscription: t): string =>
  switch (subscription) {
  | Premise(premise_id) => StoreChannel.encode(~prefix, ~id=premise_id)
  };

let decode = (value: string): option(t) =>
  switch (StoreChannel.decodeId(~prefix, ~value)) {
  | Some(premise_id) => Some(Premise(premise_id))
  | None => None
  };

let decode_channel = (value: string): option(string) =>
  switch (decode(value)) {
  | Some(subscription) => Some(channel(subscription))
  | None => None
  };
