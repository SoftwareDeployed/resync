// QueryCache.re - Client-side query cache with WebSocket subscription
// Stores type-erased data (query_result(StoreJson.json)).

open QueryRegistryTypes;

type loaded_result = {
  channel: string,
  rows: array(StoreJson.json),
};
type loaded_result_listener = loaded_result => unit;
type loaded_result_listener_id = StoreEvents.listener_id;

let loadedResultListenersRef: StoreEvents.callback_registry(loaded_result) = ref([||]);

let channelOfKey = (key: query_key): string => {
  switch (Js.String.split(~limit=2, key, ~sep=":")) {
  | [|channel, _|] => channel
  | [|channel|] => channel
  | _ => key
  };
};

let notifyLoadedResult = (~channel: string, ~rows: array(StoreJson.json)) => {
  StoreEvents.Callback.emit(
    ~registry=loadedResultListenersRef,
    {channel, rows},
  );
};

let listenLoadedResults = (listener: loaded_result_listener): loaded_result_listener_id =>
  StoreEvents.Callback.listen(~registry=loadedResultListenersRef, listener);

let unlistenLoadedResults = (listenerId: loaded_result_listener_id) =>
  StoreEvents.Callback.unlisten(~registry=loadedResultListenersRef, listenerId);

let decodeJsonRows = (json: StoreJson.json): option(array(StoreJson.json)) =>
  StoreJson.tryDecode(
    Melange_json.Primitives.array_of_json(rowJson => rowJson),
    json,
  );

// Cache entry stores type-erased JSON data
// The decoder is provided at access time, not storage time
type cache_entry = {
  key: query_key,
  mutable data: query_result(StoreJson.json),
  mutable signal: Tilia.Core.signal(query_result(StoreJson.json)),
  mutable setSignal: query_result(StoreJson.json) => unit,
  mutable subscriptionHandle: option(RealtimeClient.Socket.connection_handle),
  mutable lastUpdated: float,
  mutable refCount: int,
};

// Cache type with configurable WebSocket URLs
type t = {
  entries: Js.Dict.t(cache_entry),
  mutable eventUrl: string,
  mutable baseUrl: string,
};

// Platform-specific implementations
[@platform js]
let make = () => {
  entries: Js.Dict.empty(),
  eventUrl: "",
  baseUrl: "",
};

[@platform native]
let make = () => {
  entries: Js.Dict.empty(),
  eventUrl: "",
  baseUrl: "",
};

[@platform js]
let getOrCreateEntry =
    (~t: t, ~key: query_key, ~updatedAt: float=0.0, ()): cache_entry => {
  switch (t.entries->Js.Dict.get(key)) {
  | Some(existing) => existing
  | None =>
    let (signal, setSignal) = Tilia.Core.signal(Loading);
    let newEntry = {
      key,
      data: Loading,
      signal,
      setSignal,
      subscriptionHandle: None,
      lastUpdated: updatedAt,
      refCount: 0,
    };
    t.entries->Js.Dict.set(key, newEntry);
    newEntry;
  };
};

[@platform native]
let getOrCreateEntry =
    (~t: t, ~key: query_key, ~updatedAt as _: float=0.0, ()): cache_entry => {
  switch (t.entries->Js.Dict.get(key)) {
  | Some(existing) => existing
  | None =>
    let (signal, setSignal) = Tilia.Core.signal(Loading);
    let newEntry = {
      key,
      data: Loading,
      signal,
      setSignal,
      subscriptionHandle: None,
      lastUpdated: 0.0,
      refCount: 0,
    };
    t.entries->Js.Dict.set(key, newEntry);
    newEntry;
  };
};

let getSignal = (~t: t, ~key: query_key): Tilia.Core.signal(query_result(StoreJson.json)) => {
  let entry = getOrCreateEntry(~t, ~key, ());
  entry.signal;
};

let setEntryResult =
    (
      ~entry: cache_entry,
      ~channel: string,
      ~result: query_result(StoreJson.json),
      ~lastUpdated: float,
    ) => {
  entry.data = result;
  entry.setSignal(result);
  entry.lastUpdated = lastUpdated;
  switch (result) {
  | Loaded(rows) => notifyLoadedResult(~channel, ~rows)
  | Loading
  | Error(_) => ()
  };
};

// Configure WebSocket URLs at initialization
[@platform js]
let init = (~eventUrl: string, ~baseUrl: string, t: t) => {
  t.eventUrl = eventUrl;
  t.baseUrl = baseUrl;
};

[@platform native]
let init = (~eventUrl as _: string, ~baseUrl as _: string, _t: t) => ();

// Subscribe to a query with type-erased storage.
// Store raw JSON directly; UseQuery decodes rows on access.
[@platform js]
let subscribe =
    (
      ~t: t,
      ~key: query_key,
      ~channel: string,
      ~updatedAt: float=0.0,
      (),
    )
    : (Tilia.Core.signal(query_result(StoreJson.json)), unit => unit) => {
  let entry = getOrCreateEntry(~t, ~key, ~updatedAt, ());
  entry.refCount = entry.refCount + 1;

  // Subscribe via RealtimeClient.Socket if not already subscribed
  let handle =
    switch (entry.subscriptionHandle) {
    | Some(h) => Some(h)
    | None =>
      if (t.eventUrl != "" && t.baseUrl != "") {
        let h =
          RealtimeClient.Socket.subscribeSynced(
            ~subscription=channel,
            ~updatedAt=entry.lastUpdated,
            ~onPatch=
              (~payload as _: StoreJson.json, ~timestamp: float) => {
                let previousUpdatedAt = entry.lastUpdated;
                entry.lastUpdated = timestamp;
                let _ =
                  switch (entry.subscriptionHandle) {
                  | Some(handle) =>
                    RealtimeClient.Socket.sendFrame(
                      ~handle,
                      ~frame=RealtimeClient.selectFrameString(channel, previousUpdatedAt),
                    )
                  | None => false
                  };
                ();
              },
            ~onSnapshot=
              (json: StoreJson.json) => {
                // Store raw JSON directly; UseQuery decodes rows on access.
                let result =
                  switch (decodeJsonRows(json)) {
                  | Some(jsonRows) => Loaded(jsonRows)
                  | None => Error("Failed to decode snapshot data")
                  };
                setEntryResult(
                  ~entry,
                  ~channel,
                  ~result,
                  ~lastUpdated=Js.Date.now(),
                );
              },
            ~onAck=
              (_actionId: string, _status: string, _error: option(string)) =>
                (),
            ~onOpen=() => (),
            ~onClose=() => (),
            ~eventUrl=t.eventUrl,
            ~baseUrl=t.baseUrl,
            (),
          );
        Some(h);
      } else {
        None;
      }
    };

  // Update entry with subscription handle
  entry.subscriptionHandle = handle;

  // Return signal and unsubscribe function
  let active = ref(true);
  let unsubscribe = () => {
    if (active.contents) {
      active := false;
      entry.refCount = max(0, entry.refCount - 1);
      if (entry.refCount == 0) {
        switch (entry.subscriptionHandle) {
        | Some(h) => RealtimeClient.Socket.disposeHandle(h)
        | None => ()
        };
        entry.subscriptionHandle = None;
      };
    };
  };

  (entry.signal, unsubscribe);
};

[@platform native]
let subscribe =
    (
      ~t as _: t,
      ~key as _: query_key,
      ~channel as _: string,
      ~updatedAt as _: float=0.0,
      (),
    )
    : (Tilia.Core.signal(query_result(StoreJson.json)), unit => unit) => {
  let (signal, _setSignal) = Tilia.Core.signal(Loading);
  (signal, () => ());
};

// Get cached result (type-erased)
[@platform js]
let getResult =
    (~t: t, ~key: query_key): option(query_result(StoreJson.json)) => {
  switch (t.entries->Js.Dict.get(key)) {
  | Some(entry) => Some(entry.data)
  | None => None
  };
};

[@platform native]
let getResult =
    (~t as _: t, ~key as _: query_key): option(query_result(StoreJson.json)) => {
  None;
};

// Helper to serialize a single query result to JSON (native only for SSR)
[@platform native]
let result_to_json = (result: query_result(StoreJson.json)): StoreJson.json => {
  switch (result) {
  | Loading => StoreJson.Object.make(dict =>
      StoreJson.Object.setString(dict, "_tag", "Loading")
    )
  | Loaded(jsonArray) =>
    StoreJson.Object.make(dict => {
      StoreJson.Object.setString(dict, "_tag", "Loaded");
      StoreJson.Object.setJson(
        dict,
        "data",
        Melange_json.To_json.array(json => json)(jsonArray),
      );
    })
  | Error(msg) =>
    StoreJson.Object.make(dict => {
      StoreJson.Object.setString(dict, "_tag", "Error");
      StoreJson.Object.setString(dict, "message", msg);
    })
  };
};

// JS stub - client only decodes, doesn't encode
[@platform js]
let result_to_json = (_result: query_result(StoreJson.json)): StoreJson.json => {
  Melange_json.To_json.unit();
};

// Helper to deserialize a single query result from JSON
let result_of_json = (json: StoreJson.json): query_result(StoreJson.json) => {
  switch (StoreJson.field(json, "_tag")) {
  | Some(tagJson) =>
    switch (
      StoreJson.tryDecode(Melange_json.Primitives.string_of_json, tagJson)
    ) {
    | Some("Loading") => Loading
    | Some("Loaded") =>
      switch (StoreJson.field(json, "data")) {
      | Some(data) =>
        switch (decodeJsonRows(data)) {
        | Some(rows) => Loaded(rows)
        | None => Error("Invalid data field in Loaded result")
        }
      | None => Error("Missing data field in Loaded result")
      }
    | Some("Error") =>
      switch (StoreJson.field(json, "message")) {
      | Some(msgJson) =>
        switch (
          StoreJson.tryDecode(Melange_json.Primitives.string_of_json, msgJson)
        ) {
        | Some(msg) => Error(msg)
        | None => Error("Invalid error message")
        }
      | None => Error("Missing message field in Error result")
      }
    | _ => Error("Unknown result tag")
    }
  | None => Error("Missing _tag field")
  };
};

let forEachSerializedResult =
    (
      ~jsonStr: string,
      ~f:
        (
          ~key: query_key,
          ~result: query_result(StoreJson.json),
        ) =>
        unit,
    )
    : unit => {
  switch (StoreJson.tryParse(jsonStr)) {
  | Some(json) =>
    let dict = StoreJson.Dict.of_json(x => x, json);
    let entries = Js.Dict.entries(dict);
    for (i in 0 to Array.length(entries) - 1) {
      let (key, resultJson) = entries[i];
      f(~key, ~result=result_of_json(resultJson));
    };
  | None => ()
  };
};

let forEachLoadedResult = (~jsonStr: string, ~f: loaded_result_listener): unit =>
  forEachSerializedResult(
    ~jsonStr,
    ~f=(~key, ~result) =>
      switch (result) {
      | Loaded(rows) => f({channel: channelOfKey(key), rows})
      | Loading
      | Error(_) => ()
      },
  );

// Hydrate cache from SSR-serialized JSON
[@platform js]
let hydrate = (~t: t, ~jsonStr: string): unit => {
  forEachSerializedResult(
    ~jsonStr,
    ~f=(~key, ~result) => {
      let entry = getOrCreateEntry(~t, ~key, ());
      setEntryResult(
        ~entry,
        ~channel=channelOfKey(key),
        ~result,
        ~lastUpdated=entry.lastUpdated,
      );
    },
  );
};

[@platform native]
let hydrate = (~t as _: t, ~jsonStr as _: string) => ();

// Serialize cache to JSON string for SSR (native only - server serializes)
[@platform native]
let serialize = (t: t): string => {
  let dict = Js.Dict.empty();
  let entries = Js.Dict.entries(t.entries);
  for (i in 0 to Array.length(entries) - 1) {
    let (key, entry) = entries[i];
    dict->Js.Dict.set(key, result_to_json(entry.data));
  };
  StoreJson.stringify(json => json, StoreJson.Dict.to_json(json => json, dict));
};

[@platform js]
let serialize = (_t: t): string => "";
