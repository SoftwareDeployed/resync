// QueryCache.re - Client-side query cache with WebSocket subscription
// Stores type-erased data (query_result(StoreJson.json)) to avoid Obj.magic

open QueryRegistryTypes;

type loaded_result_listener = (~channel: string, ~rows: array(StoreJson.json)) => unit;
type loaded_result_listener_id = string;

let loadedResultListenersRef: ref(array((loaded_result_listener_id, loaded_result_listener))) = ref([||]);

let channelOfKey = (key: query_key): string => {
  switch (Js.String.split(~limit=2, key, ~sep=":")) {
  | [|channel, _|] => channel
  | [|channel|] => channel
  | _ => key
  };
};

let notifyLoadedResult = (~channel: string, ~rows: array(StoreJson.json)) => {
  loadedResultListenersRef.contents
  ->Js.Array.forEach(~f=((_, listener)) => listener(~channel, ~rows));
};

let listenLoadedResults = (listener: loaded_result_listener): loaded_result_listener_id => {
  let listenerId = UUID.make();
  loadedResultListenersRef.contents =
    Js.Array.concat(
      ~other=[|(listenerId, listener)|],
      loadedResultListenersRef.contents,
    );
  listenerId;
};

let unlistenLoadedResults = (listenerId: loaded_result_listener_id) => {
  loadedResultListenersRef.contents =
    loadedResultListenersRef.contents->Js.Array.filter(~f=((currentId, _)) =>
      currentId != listenerId
    );
};

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

// External for deleting dictionary entries (already in file)
[@platform js]
external deleteEntry: (Js.Dict.t('a), string) => unit = "delete";

[@platform native]
let deleteEntry = (_dict, _key) => ();

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

// Configure WebSocket URLs at initialization
[@platform js]
let init = (~eventUrl: string, ~baseUrl: string, t: t) => {
  t.eventUrl = eventUrl;
  t.baseUrl = baseUrl;
};

[@platform native]
let init = (~eventUrl as _: string, ~baseUrl as _: string, _t: t) => ();

// Subscribe to a query with type-erased storage
// Store raw JSON directly - decodeRow is used by consumers, not the cache
[@platform js]
let subscribe =
    (
      ~t: t,
      ~key: query_key,
      ~channel: string,
      ~decodeRow as _: StoreJson.json => 'row,
      ~updatedAt: float=0.0,
      (),
    )
    : (Tilia.Core.signal(query_result(StoreJson.json)), unit => unit) => {
  // Get or create cache entry
  let entry =
    switch (t.entries->Js.Dict.get(key)) {
    | Some(existing) =>
      // Increment ref count and return existing entry
      if (existing.refCount == 0) {
        let (signal, setSignal) = Tilia.Core.signal(existing.data);
        existing.signal = signal;
        existing.setSignal = setSignal;
      };
      existing.refCount = existing.refCount + 1;
      existing;
    | None =>
      // Create new entry with Loading state
      let (signal, setSignal) = Tilia.Core.signal(Loading);
      let newEntry = {
        key,
        data: Loading,
        signal,
        setSignal,
        subscriptionHandle: None,
        lastUpdated: updatedAt,
        refCount: 1,
      };
      t.entries->Js.Dict.set(key, newEntry);
      newEntry;
    };

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
                // For now, just update lastUpdated timestamp
                // Patch application will be enhanced later
                entry.lastUpdated = timestamp
              },
            ~onSnapshot=
              (json: StoreJson.json) => {
                // Store raw JSON directly - the cache stores type-erased JSON
                // UseQuery will decode using decodeRow on access
                let result =
                  switch (
                    StoreJson.tryDecode(
                      Melange_json.Primitives.array_of_json(x => x),
                      json,
                    )
                  ) {
                  | Some(jsonRows) => Loaded(jsonRows)
                  | None => Error("Failed to decode snapshot data")
                  };
                entry.data = result;
                entry.setSignal(result);
                entry.lastUpdated = Js.Date.now();
                switch (result) {
                | Loaded(jsonRows) => notifyLoadedResult(~channel, ~rows=jsonRows)
                | Loading
                | Error(_) => ()
                };
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
  let unsubscribe = () => {
    entry.refCount = entry.refCount - 1;
    if (entry.refCount <= 0) {
      // Dispose subscription
      switch (entry.subscriptionHandle) {
      | Some(h) => RealtimeClient.Socket.disposeHandle(h)
      | None => ()
      };
      // Remove entry from cache
      deleteEntry(t.entries, key);
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
      ~decodeRow as _: StoreJson.json => 'row,
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
  | Loading => `Assoc([("_tag", `String("Loading"))])
  | Loaded(jsonArray) =>
    `Assoc([
      ("_tag", `String("Loaded")),
      ("data", `List(Belt.List.fromArray(jsonArray))),
    ])
  | Error(msg) =>
    `Assoc([("_tag", `String("Error")), ("message", `String(msg))])
  };
};

// JS stub - client only decodes, doesn't encode
[@platform js]
let result_to_json = (_result: query_result(StoreJson.json)): StoreJson.json => {
  Obj.magic(Js.Json.null);
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
        switch (
          StoreJson.tryDecode(
            Melange_json.Primitives.array_of_json(x => x),
            data,
          )
        ) {
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

let forEachLoadedResult = (~jsonStr: string, ~f: loaded_result_listener): unit => {
  switch (StoreJson.tryParse(jsonStr)) {
  | Some(json) =>
    let dict = StoreJson.Dict.of_json(x => x, json);
    let entries = Js.Dict.entries(dict);
    for (i in 0 to Array.length(entries) - 1) {
      let (key, resultJson) = entries[i];
      switch (result_of_json(resultJson)) {
      | Loaded(rows) => f(~channel=channelOfKey(key), ~rows)
      | Loading
      | Error(_) => ()
      };
    };
  | None => ()
  };
};

// Hydrate cache from SSR-serialized JSON
[@platform js]
let hydrate = (~t: t, ~jsonStr: string): unit => {
  switch (StoreJson.tryParse(jsonStr)) {
  | Some(json) =>
    // Parse as object/dict using StoreJson.Dict module
    let dict = StoreJson.Dict.of_json(x => x, json);
    let entries = Js.Dict.entries(dict);
    for (i in 0 to Array.length(entries) - 1) {
      let (key, resultJson) = entries[i];
      let result = result_of_json(resultJson);
      let (signal, setSignal) = Tilia.Core.signal(result);
      let entry = {
        key,
        data: result,
        signal,
        setSignal,
        subscriptionHandle: None,
        lastUpdated: 0.0,
        refCount: 0 // Will be incremented when subscribe is called
      };
      t.entries->Js.Dict.set(key, entry);
      switch (result) {
      | Loaded(rows) => notifyLoadedResult(~channel=channelOfKey(key), ~rows)
      | Loading
      | Error(_) => ()
      };
    };
  | None => ()
  };
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
  Yojson.Basic.to_string(
    `Assoc(
      Js.Dict.entries(dict)
      |> Array.to_list
      |> List.map(((k, v)) => (k, Obj.magic(v))),
    ),
  );
};

[@platform js]
let serialize = (_t: t): string => "";
