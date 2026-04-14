// QueryCache.re - Client-side query cache with WebSocket subscription

open QueryRegistryTypes;

type query_entry('row) = {
  key: query_key,
  signal: Tilia.Core.signal(query_result('row)),
  setResult: query_result('row) => unit,
  subscriptionHandle: option(RealtimeClientMultiplexed.Multiplexed.subscription_handle),
  mutable lastUpdated: float,
};

type t('row) = {
  entriesRef: ref(Js.Dict.t(query_entry('row))),
  clientRef: ref(option(RealtimeClientMultiplexed.Multiplexed.t)),
};

[@platform js]
external deleteEntry: (Js.Dict.t('a), string) => unit = "delete";

[@platform native]
let deleteEntry = (_dict, _key) => ();

let make = () => {
  entriesRef: ref(Js.Dict.empty()),
  clientRef: ref(None),
};

let setClient = (t, client) => {
  t.clientRef := Some(client);
};

[@platform js]
let subscribe = (
  ~t: t('row),
  ~channel: string,
  ~updatedAt: float=0.0,
  ~onPatch: (~payload: StoreJson.json, ~timestamp: float) => unit,
  ~onSnapshot: StoreJson.json => unit,
  ~decodeRow as _: StoreJson.json => 'row,
  (),
) : (Tilia.Core.signal(query_result('row)), unit => unit) => {
  let (signal, setSignal) = Tilia.Core.signal(Loading);
  let entry = {
    key: channel,
    signal,
    setResult: setSignal,
    subscriptionHandle: None,
    lastUpdated: updatedAt,
  };
  let entries = t.entriesRef.contents;
  entries->Js.Dict.set(channel, entry);
  t.entriesRef := entries;
  
  // Subscribe via WebSocket if client available
  let handle = t.clientRef.contents |> Option.map(client => {
    RealtimeClientMultiplexed.Multiplexed.subscribe(
      ~channel,
      ~updatedAt,
      ~onPatch,
      ~onSnapshot,
      ~onAck=(_actionId, _status, _error) => (),
      ~onOpen=() => (),
      ~onClose=() => (),
      client,
    )
  });
  
  let entryWithHandle = {...entry, subscriptionHandle: handle};
  t.entriesRef.contents->Js.Dict.set(channel, entryWithHandle);
  
  let unsubscribe = () => {
    switch (handle) {
    | Some(h) => t.clientRef.contents |> Option.iter(client => {
      RealtimeClientMultiplexed.Multiplexed.unsubscribe(client, h);
    });
    | None => ()
    };
    deleteEntry(t.entriesRef.contents, channel);
  };
  
  (signal, unsubscribe);
};

[@platform native]
let subscribe = (
  ~t as _: t('row),
  ~channel as _: string,
  ~updatedAt as _: float=0.0,
  ~onPatch as _: (~payload: StoreJson.json, ~timestamp: float) => unit,
  ~onSnapshot as _: StoreJson.json => unit,
  ~decodeRow as _: StoreJson.json => 'row,
  (),
) : (Tilia.Core.signal(query_result('row)), unit => unit) => {
  let (signal, _setSignal) = Tilia.Core.signal(Loading);
  (signal, () => ());
};

let updateResult = (t, ~key: query_key, ~result: query_result('row)) => {
  switch (t.entriesRef.contents->Js.Dict.get(key)) {
  | Some(entry) => 
      entry.setResult(result);
      entry.lastUpdated = Js.Date.now();
  | None => ()
  };
};

let getResult = (t, ~key: query_key): option(query_result('row)) => {
  t.entriesRef.contents
  ->Js.Dict.get(key)
  |> Option.map(entry => Tilia.Core.lift(entry.signal));
};
