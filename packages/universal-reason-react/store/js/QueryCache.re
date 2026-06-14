// QueryCache.re - Client-side query cache with WebSocket subscription
// Stores type-erased data (query_result(StoreJson.json)).

open QueryRegistryTypes;

type loaded_result = {
  key: query_key,
  channel: string,
  rows: array(StoreJson.json),
};
type loaded_result_listener = loaded_result => unit;
type loaded_result_listener_id = StoreEvents.listener_id;

type subscription_handle =
  | Standalone(RealtimeClient.Socket.connection_handle)
  | Shared(
      RealtimeClientMultiplexed.Multiplexed.t,
      RealtimeClientMultiplexed.Multiplexed.subscription_handle,
    );

let notifyLoadedResult =
    (
      ~registry: StoreEvents.callback_registry(loaded_result),
      ~key: query_key,
      ~channel: string,
      ~rows: array(StoreJson.json),
    ) => {
  StoreEvents.Callback.emit(
    ~registry,
    {key, channel, rows},
  );
};

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
  mutable subscriptionHandle: option(subscription_handle),
  mutable subscriptionChannel: option(string),
  mutable lastUpdated: float,
  mutable resyncTimeoutId: option(int),
  mutable refCount: int,
};

// Cache type with configurable WebSocket URLs
type t = {
  entries: Js.Dict.t(cache_entry),
  mutable eventUrl: string,
  mutable baseUrl: string,
  mutable sharedConnection: option(RealtimeClientMultiplexed.Multiplexed.t),
  loadedResultListenersRef: StoreEvents.callback_registry(loaded_result),
};

// Platform-specific implementations
[@platform js]
let make = () => {
  entries: Js.Dict.empty(),
  eventUrl: "",
  baseUrl: "",
  sharedConnection: None,
  loadedResultListenersRef: ref([||]),
};

[@platform native]
let make = () => {
  entries: Js.Dict.empty(),
  eventUrl: "",
  baseUrl: "",
  sharedConnection: None,
  loadedResultListenersRef: ref([||]),
};

let listenLoadedResults = (~t: t, listener: loaded_result_listener): loaded_result_listener_id =>
  StoreEvents.Callback.listen(~registry=t.loadedResultListenersRef, listener);

let unlistenLoadedResults = (~t: t, listenerId: loaded_result_listener_id) =>
  StoreEvents.Callback.unlisten(~registry=t.loadedResultListenersRef, listenerId);

let shouldUseTransport = (~eventUrl: string, ~baseUrl as _: string) =>
  eventUrl != "";

[@platform js]
external setTimeout: (unit => unit, int) => int = "setTimeout";
[@platform js]
external clearTimeout: int => unit = "clearTimeout";

[@platform js]
let clearPendingResync = (entry: cache_entry) => {
  switch (entry.resyncTimeoutId) {
  | Some(timeoutId) =>
    clearTimeout(timeoutId);
    entry.resyncTimeoutId = None;
  | None => ()
  };
};

[@platform native]
let clearPendingResync = (_entry: cache_entry) => ();

module InternalForTests = {
  let loadedResultListenerCount = (~t: t) =>
    Array.length(t.loadedResultListenersRef.contents);

  let shouldUseTransport = shouldUseTransport;
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
      subscriptionChannel: None,
      lastUpdated: updatedAt,
      resyncTimeoutId: None,
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
      subscriptionChannel: None,
      lastUpdated: 0.0,
      resyncTimeoutId: None,
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
      ~t: t,
      ~entry: cache_entry,
      ~channel: string,
      ~result: query_result(StoreJson.json),
      ~lastUpdated: float,
    ) => {
  entry.data = result;
  entry.setSignal(result);
  entry.lastUpdated = lastUpdated;
  switch (result) {
  | Loaded(rows) =>
    notifyLoadedResult(
      ~registry=t.loadedResultListenersRef,
      ~key=entry.key,
      ~channel,
      ~rows,
    )
  | Loading
  | Error(_) => ()
  };
};

let isLoading = (result: query_result('row)) =>
  switch (result) {
  | Loading => true
  | Loaded(_)
  | Error(_) => false
  };

let uninitializedTransportMessage =
  "QueryCache transport is not initialized. Call UseQuery.initCache before subscribing to uncached queries.";

// Configure WebSocket URLs at initialization
[@platform js]
let init = (~eventUrl: string, ~baseUrl: string, t: t) => {
  t.eventUrl = eventUrl;
  t.baseUrl = baseUrl;
};

[@platform native]
let init = (~eventUrl as _: string, ~baseUrl as _: string, _t: t) => ();

[@platform js]
let sendSelectFrame = (~handle: subscription_handle, ~channel: string, ~updatedAt: float) =>
  switch (handle) {
  | Standalone(socketHandle) =>
    RealtimeClient.Socket.sendFrame(
      ~handle=socketHandle,
      ~frame=RealtimeClient.selectFrameString(channel, updatedAt),
    )
  | Shared(multiplexed, _) =>
    RealtimeClientMultiplexed.Multiplexed.sendFrame(
      ~frame=RealtimeClient.selectFrameString(channel, updatedAt),
      multiplexed,
    )
  };

[@platform js]
let disposeSubscription = (handle: subscription_handle) =>
  switch (handle) {
  | Standalone(socketHandle) => RealtimeClient.Socket.disposeHandle(socketHandle)
  | Shared(multiplexed, subscriptionHandle) =>
    RealtimeClientMultiplexed.Multiplexed.unsubscribe(
      multiplexed,
      subscriptionHandle,
    )
  };

[@platform js]
let subscribeTransport =
    (~t: t, ~entry: cache_entry, ~channel: string)
    : option(subscription_handle) => {
  let onPatch = (~payload as _: StoreJson.json, ~timestamp: float) => {
    let previousUpdatedAt = entry.lastUpdated;
    entry.lastUpdated = timestamp;
    clearPendingResync(entry);
    entry.resyncTimeoutId =
      Some(
        setTimeout(
          () => {
            entry.resyncTimeoutId = None;
            let _ =
              switch (entry.subscriptionHandle) {
              | Some(handle) =>
                sendSelectFrame(~handle, ~channel, ~updatedAt=previousUpdatedAt)
              | None => false
              };
            ();
          },
          100,
        ),
      );
    ();
  };

  let onSnapshot = (json: StoreJson.json) => {
    clearPendingResync(entry);
    // Store raw JSON directly; UseQuery decodes rows on access.
    let result =
      switch (decodeJsonRows(json)) {
      | Some(jsonRows) => Loaded(jsonRows)
      | None => Error("Failed to decode snapshot data")
      };
    setEntryResult(
      ~t,
      ~entry,
      ~channel,
      ~result,
      ~lastUpdated=Js.Date.now(),
    );
  };

  switch (t.sharedConnection) {
  | Some(multiplexed) =>
    let handle =
      RealtimeClientMultiplexed.Multiplexed.subscribe(
        ~channel,
        ~updatedAt=entry.lastUpdated,
        ~onPatch,
        ~onSnapshot,
        ~onAck=(_actionId: string, _status: string, _error: option(string)) =>
          (),
        ~onOpen=() => (),
        ~onClose=() => (),
        multiplexed,
      );
    Some(Shared(multiplexed, handle));
  | None =>
    if (shouldUseTransport(~eventUrl=t.eventUrl, ~baseUrl=t.baseUrl)) {
      let handle =
        RealtimeClient.Socket.subscribeSynced(
          ~subscription=channel,
          ~updatedAt=entry.lastUpdated,
          ~onPatch,
          ~onSnapshot,
          ~onAck=(_actionId: string, _status: string, _error: option(string)) =>
            (),
          ~onOpen=() => (),
          ~onClose=() => (),
          ~eventUrl=t.eventUrl,
          ~baseUrl=t.baseUrl,
          (),
        );
      Some(Standalone(handle));
    } else {
      if (isLoading(entry.data)) {
        setEntryResult(
          ~t,
          ~entry,
          ~channel,
          ~result=Error(uninitializedTransportMessage),
          ~lastUpdated=entry.lastUpdated,
        );
      };
      None;
    }
  };
};

[@platform js]
let setConnectionHandle =
    (
      ~t: t,
      handle: option(RealtimeClientMultiplexed.Multiplexed.t),
    ) => {
  t.sharedConnection = handle;
  switch (handle) {
  | Some(_) =>
    t.entries
    ->Js.Dict.entries
    ->Js.Array.forEach(~f=((_, entry)) => {
        if (entry.refCount > 0) {
          clearPendingResync(entry);
          switch (entry.subscriptionHandle) {
          | Some(existing) => disposeSubscription(existing)
          | None => ()
          };
          entry.subscriptionHandle =
            switch (entry.subscriptionChannel) {
            | Some(channel) => subscribeTransport(~t, ~entry, ~channel)
            | None => None
            };
        };
      })
  | None => ()
  };
};

[@platform native]
let setConnectionHandle =
    (
      ~t as _: t,
      _handle: option(RealtimeClientMultiplexed.Multiplexed.t),
    ) => ();

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
  entry.subscriptionChannel = Some(channel);

  // Subscribe through the store-owned multiplexed connection when available.
  let handle =
    switch (entry.subscriptionHandle) {
    | Some(h) => Some(h)
    | None => subscribeTransport(~t, ~entry, ~channel)
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
        clearPendingResync(entry);
        switch (entry.subscriptionHandle) {
        | Some(h) => disposeSubscription(h)
        | None => ()
        };
        entry.subscriptionHandle = None;
        entry.subscriptionChannel = None;
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
      | Loaded(rows) => f({key, channel: channelOfKey(key), rows})
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
        ~t,
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
