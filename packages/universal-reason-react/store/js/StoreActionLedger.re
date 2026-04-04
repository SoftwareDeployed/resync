type status =
  | Pending
  | Syncing
  | Acked
  | Failed;

type t = StoreIndexedDB.action_record;

let ackTimeoutMs = 5000;
let maxRetries = 3;

let statusToString = status =>
  switch (status) {
  | Pending => "pending"
  | Syncing => "syncing"
  | Acked => "acked"
  | Failed => "failed"
  };

let statusOfString = status =>
  switch (status) {
  | "pending" => Pending
  | "syncing" => Syncing
  | "acked" => Acked
  | "failed" => Failed
  | _ => Pending
  };

let make = (~id: string, ~scopeKey: string, ~action: StoreJson.json, ()) : t => {
  id,
  scopeKey,
  action,
  status: statusToString(Pending),
  enqueuedAt: Js.Date.now(),
  retryCount: 0,
  error: None,
};

let put = (~storeName: string, record: t) =>
  StoreIndexedDB.putAction(~name=storeName, record);

let get = (~storeName: string, ~id: string, ()) =>
  StoreIndexedDB.getAction(~name=storeName, ~id, ());

let getByScope = (~storeName: string, ~scopeKey: string, ()) =>
  StoreIndexedDB.getActionsByScope(~name=storeName, ~scopeKey, ());

let updateStatus = (~storeName: string, ~id: string, ~status: status, ~error: option(string)=?, ()) =>
  Js.Promise.then_(
    current =>
      switch (current) {
      | Some(record) =>
        put(
          ~storeName,
          {
            ...record,
            status: statusToString(status),
            error,
          },
        )
      | None => Js.Promise.resolve()
      },
    get(~storeName, ~id, ()),
  );

let deleteByIds = (~storeName: string, ~ids: array(string), ()) =>
  StoreIndexedDB.deleteActions(~name=storeName, ~ids, ());

[@platform js]
let sortByEnqueuedAt: array(t) => array(t) =
  [%raw
    {|
  function(records) {
    return records.slice().sort(function(left, right) {
      return left.enqueuedAt - right.enqueuedAt;
    });
  }
  |}];

[@platform native]
let sortByEnqueuedAt = records => records;
