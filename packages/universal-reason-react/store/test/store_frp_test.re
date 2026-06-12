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

let schema: StoreBuilder.schemaConfig(state, action, store) = {
  emptyState: state,
  reduce: (~state, ~action as _) => state,
  makeStore,
};

let transport: StoreBuilder.Sync.transportConfig(state, string) = {
  subscriptionOfState: _ => None,
  encodeSubscription: value => value,
  eventUrl: "",
  baseUrl: "",
};

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

let withBaseJson = builder =>
  builder
  |> StoreBuilder.withJson(
       ~state_of_json=_ => state,
       ~state_to_json=json,
       ~action_of_json=_ => Inc,
       ~action_to_json=json,
     );

let withOptionalGuardTree = (~guardTree, builder) =>
  switch (guardTree) {
  | Some(guardTree) => builder |> StoreBuilder.withGuardTree(~guardTree)
  | None => builder
  };

let withOptionalQueries = (~applyQueryResult, builder) =>
  switch (applyQueryResult) {
  | Some(applyQueryResult) =>
    builder |> StoreBuilder.withQueries(~applyQueryResult)
  | None => builder
  };

let makeBasePipeline = (~guardTree=?, ()) =>
  StoreBuilder.make()
  |> StoreBuilder.withSchema(schema)
  |> withOptionalGuardTree(~guardTree);

let localInput = (~guardTree=?, ~applyQueryResult=?, ()) =>
  makeBasePipeline(~guardTree?, ())
  |> withBaseJson
  |> withOptionalQueries(~applyQueryResult)
  |> StoreBuilder.withLocalPersistence(
       ~storeName="frp-local-test",
       ~scopeKeyOfState=_ => "default",
       ~timestampOfState=_ => 0.0,
       ~stateElementId=None,
       (),
     );

let syncedInput = (~guardTree=?, ~applyQueryResult=?, ()) =>
  makeBasePipeline(~guardTree?, ())
  |> withBaseJson
  |> withOptionalQueries(~applyQueryResult)
  |> StoreBuilder.withSync(
       ~transport,
       ~decodePatch=(_json: StoreJson.json): option(string) => None,
       ~updateOfPatch=(_patch: string, state) => state,
       ~setTimestamp=(~state, ~timestamp as _) => state,
       ~storeName="frp-synced-test",
       ~scopeKeyOfState=_ => "default",
       ~timestampOfState=_ => 0.0,
       ~emptyStreamingState=(),
       ~stateElementId=None,
       (),
     );

let streamingInput = (~streams, ()) =>
  makeBasePipeline()
  |> withBaseJson
  |> StoreBuilder.withSync(
       ~transport,
       ~decodePatch=(_json: StoreJson.json): option(string) => None,
       ~updateOfPatch=(_patch: string, state) => state,
       ~setTimestamp=(~state, ~timestamp as _) => state,
       ~storeName="frp-synced-streaming-test",
       ~scopeKeyOfState=_ => "default",
       ~timestampOfState=_ => 0.0,
       ~streams=Some(streams),
       ~stateElementId=None,
       (),
     );

let crudInput = (~guardTree=?, ~applyQueryResult=?, ()) =>
  makeBasePipeline(~guardTree?, ())
  |> withBaseJson
  |> withOptionalQueries(~applyQueryResult)
  |> StoreBuilder.withSyncCrud(
       ~transport,
       ~setTimestamp=(~state, ~timestamp as _) => state,
       ~storeName="frp-crud-test",
       ~scopeKeyOfState=_ => "default",
       ~timestampOfState=_ => 0.0,
       ~table="items",
       ~decodeRow=_ => {id: ""},
       ~getId=row => row.id,
       ~getItems=_ => [||],
       ~setItems=(state, _rows) => state,
       ~stateElementId=None,
       (),
     );

let buildLocal = input => {
  let _ = StoreBuilder.buildLocal(input);
  ();
};

let buildSynced = input => {
  let _ = StoreBuilder.buildSynced(input);
  ();
};

let buildCrud = input => {
  let _ = StoreBuilder.buildCrud(input);
  ();
};

let assertStreamingInputAppliesStream = (~label, ~input, expected) => {
  switch (input.StoreBuilder.streams) {
  | Some(streams) => assertAppliesStream(~label, streams, expected)
  | None => Alcotest.fail(label ++ " did not preserve streams config")
  };
};

let suite =
  (
    "StoreBuilder",
    [
      Alcotest.test_case("buildLocal pipeline preserves guard tree", `Quick, () => {
        let input = localInput(~guardTree, ());
        assertBlocks(~label="buildLocal pipeline", input.guardTree);
        buildLocal(input);
      }),
      Alcotest.test_case("buildSynced pipeline preserves guard tree", `Quick, () => {
        let input = syncedInput(~guardTree, ());
        assertBlocks(~label="buildSynced pipeline", input.guardTree);
        buildSynced(input);
      }),
      Alcotest.test_case("buildCrud pipeline preserves guard tree", `Quick, () => {
        let input = crudInput(~guardTree, ());
        assertBlocks(~label="buildCrud pipeline", input.guardTree);
        buildCrud(input);
      }),
      Alcotest.test_case("buildLocal pipeline preserves queries config", `Quick, () => {
        let input = localInput(~applyQueryResult, ());
        assertAppliesQueryResult(~label="buildLocal pipeline", input.queries);
        buildLocal(input);
      }),
      Alcotest.test_case("buildSynced pipeline preserves queries config", `Quick, () => {
        let input = syncedInput(~applyQueryResult, ());
        assertAppliesQueryResult(~label="buildSynced pipeline", input.queries);
        buildSynced(input);
      }),
      Alcotest.test_case("buildCrud pipeline preserves queries config", `Quick, () => {
        let input = crudInput(~applyQueryResult, ());
        assertAppliesQueryResult(~label="buildCrud pipeline", input.queries);
        buildCrud(input);
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
      Alcotest.test_case("buildSynced streaming pipeline preserves streams config", `Quick, () => {
        let input = streamingInput(~streams, ());
        assertStreamingInputAppliesStream(
          ~label="buildSynced streaming pipeline",
          ~input,
          "token",
        );
        buildSynced(input);
      }),
      Alcotest.test_case("buildSynced streaming pipeline accepts replacement streams", `Quick, () => {
        let input = streamingInput(~streams=prefixedStreams, ());
        assertAppliesStream(
          ~label="buildSynced streaming replacement",
          switch (input.StoreBuilder.streams) {
          | Some(streams) => streams
          | None => Alcotest.fail("buildSynced streaming replacement did not preserve streams config")
          },
          "prefix:token",
        );
        buildSynced(input);
      }),
    ],
  );
