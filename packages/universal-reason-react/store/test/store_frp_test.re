open Store;

type state = {count: int};
type action =
  | Inc
  | Blocked;
type store = {state: state};
type row = {id: string};
type stream_event = Append(string);
type streaming_state = {text: string};

let state = {count: 0};
let emptyStreamingState = {text: ""};

let guardTree =
  StoreBuilder.GuardTree.denyIf(
    ~predicate=action =>
      switch (action) {
      | Blocked => true
      | Inc => false
      },
    ~reason="blocked",
    (),
  );

let makeStore = (~state, ~derive=?, ()): store => {
  let _ = derive;
  {state: state};
};

let storeContext = React.createContext(makeStore(~state, ()));

module TestProvider = {
  type props = Js.t({. value: store, children: React.element});
  let make = (props: props) => React.Context.provider(storeContext)(props);
};

let json = _ => Json.parse("{}");

let localSchema: Frp.Local.schema(state, action, store) = {
  storeName: "frp-local-test",
  emptyState: state,
  reduce: (~state, ~action as _) => state,
  state_of_json: _ => state,
  state_to_json: json,
  action_of_json: _ => Inc,
  action_to_json: json,
  makeStore,
  scopeKeyOfState: _ => "default",
  timestampOfState: _ => 0.0,
  stateElementId: None,
};

let syncedSchema: Frp.Synced.schema(state, action, store) = {
  storeName: "frp-synced-test",
  emptyState: state,
  reduce: (~state, ~action as _) => state,
  state_of_json: _ => state,
  state_to_json: json,
  action_of_json: _ => Inc,
  action_to_json: json,
  makeStore,
  scopeKeyOfState: _ => "default",
  timestampOfState: _ => 0.0,
  setTimestamp: (~state, ~timestamp as _) => state,
  stateElementId: None,
};

let crudSchema: Frp.Crud.schema(state, action, store) = {
  storeName: "frp-crud-test",
  emptyState: state,
  reduce: (~state, ~action as _) => state,
  state_of_json: _ => state,
  state_to_json: json,
  action_of_json: _ => Inc,
  action_to_json: json,
  makeStore,
  scopeKeyOfState: _ => "default",
  timestampOfState: _ => 0.0,
  setTimestamp: (~state, ~timestamp as _) => state,
  stateElementId: None,
};

let transport: StoreBuilder.Sync.transportConfig(state, string) = {
  subscriptionOfState: _ => None,
  encodeSubscription: value => value,
  eventUrl: "",
  baseUrl: "",
};

let customStrategy: StoreBuilder.Sync.customStrategy(state, string) =
  StoreBuilder.Sync.custom(
    ~decodePatch=_ => None,
    ~updateOfPatch=(_patch, state) => state,
  );

let crudStrategy: StoreBuilder.Sync.crudStrategy(state, row) =
  StoreBuilder.Sync.crud(
    ~table="items",
    ~decodeRow=_ => {id: ""},
    ~getId=row => row.id,
    ~getItems=_ => [||],
    ~setItems=(state, _rows) => state,
  );

let streamsWithPrefix = (prefix): StoreRuntimeTypes.streamsConfig(string, stream_event, streaming_state) => {
  decodeStreamEvent: _ => None,
  emptyStreamingState,
  reduceStream: (streaming, event) =>
    switch (event) {
    | Append(value) => {text: streaming.text ++ prefix ++ value}
    },
  reconcilePatch: (_patch, streaming) => streaming,
};

let streams = streamsWithPrefix("");
let prefixedStreams = streamsWithPrefix("prefix:");

let applyQueryResult = (~state, ~channel as _, ~rows as _) => {
  count: state.count + 1,
};

let assertBlocks = (~label, guardTree) => {
  switch (StoreBuilder.validateOfGuardTree(guardTree)) {
  | Some(validate) =>
    switch (validate(~state, ~action=Blocked)) {
    | StoreRuntimeTypes.Deny("blocked") => ()
    | StoreRuntimeTypes.Deny(reason) =>
      Alcotest.fail(label ++ " denied with unexpected reason: " ++ reason)
    | StoreRuntimeTypes.Allow =>
      Alcotest.fail(label ++ " allowed a guarded action")
    }
  | None => Alcotest.fail(label ++ " did not preserve a guard tree")
  };
};

let assertAppliesQueryResult = (
  ~label,
  queries: option(StoreBuilder.queriesConfig(state)),
) => {
  switch (queries) {
  | Some(config) =>
    let next =
      config.StoreOffline.Local.applyQueryResult(
        ~state,
        ~channel="test",
        ~rows=[||],
      );
    Alcotest.check(Alcotest.int, label ++ " result count", 1, next.count);
  | None => Alcotest.fail(label ++ " did not preserve queries config")
  };
};

let assertAppliesStream = (~label, streams, expected) => {
  let next =
    streams.StoreRuntimeTypes.reduceStream(emptyStreamingState, Append("token"));
  Alcotest.check(Alcotest.string, label ++ " text", expected, next.text);
};

let suite =
  (
    "StoreFrp",
    [
      Alcotest.test_case("local make preserves guard tree", `Quick, () => {
        let config = Frp.Local.make(~guardTree, localSchema);
        assertBlocks(~label="local make", config.guardTree);
      }),
      Alcotest.test_case("synced withGuardTree preserves guard tree", `Quick, () => {
        let config =
          Frp.Synced.make(~transport, ~strategy=customStrategy, syncedSchema)
          |> Frp.Synced.withGuardTree(~guardTree);
        assertBlocks(~label="synced withGuardTree", config.guardTree);
      }),
      Alcotest.test_case("crud withGuardTree preserves guard tree", `Quick, () => {
        let config =
          Frp.Crud.make(~transport, ~strategy=crudStrategy, crudSchema)
          |> Frp.Crud.withGuardTree(~guardTree);
        assertBlocks(~label="crud withGuardTree", config.guardTree);
      }),
      Alcotest.test_case("local make preserves queries config", `Quick, () => {
        let config = Frp.Local.make(~applyQueryResult, localSchema);
        assertAppliesQueryResult(~label="local make", config.queries);
      }),
      Alcotest.test_case("synced withQueries preserves queries config", `Quick, () => {
        let config =
          Frp.Synced.make(~transport, ~strategy=customStrategy, syncedSchema)
          |> Frp.Synced.withQueries(~applyQueryResult);
        assertAppliesQueryResult(~label="synced withQueries", config.queries);
      }),
      Alcotest.test_case("crud withQueries preserves queries config", `Quick, () => {
        let config =
          Frp.Crud.make(~transport, ~strategy=crudStrategy, crudSchema)
          |> Frp.Crud.withQueries(~applyQueryResult);
        assertAppliesQueryResult(~label="crud withQueries", config.queries);
      }),
      Alcotest.test_case("native withCreatedProvider creates provider", `Quick, () => {
        let result =
          StoreBuilder.Bootstrap.withCreatedProvider(
            ~createStore=(initialState: state) => {state: initialState},
            ~provider=TestProvider.make,
            ~initialState={count: 3},
            ~children=React.null,
          );
        Alcotest.check(
          Alcotest.int,
          "created store count",
          3,
          result.store.state.count,
        );
      }),
      Alcotest.test_case("synced streaming make preserves streams config", `Quick, () => {
        let config =
          Frp.Synced.Streaming.make(
            ~transport,
            ~strategy=customStrategy,
            ~streams,
            syncedSchema,
          );
        assertAppliesStream(~label="synced streaming make", config.streams, "token");
      }),
      Alcotest.test_case("synced streaming withStreams replaces streams config", `Quick, () => {
        let config =
          Frp.Synced.Streaming.make(
            ~transport,
            ~strategy=customStrategy,
            ~streams,
            syncedSchema,
          )
          |> Frp.Synced.Streaming.withStreams(~streams=prefixedStreams);
        assertAppliesStream(
          ~label="synced streaming withStreams",
          config.streams,
          "prefix:token",
        );
      }),
    ],
  );
