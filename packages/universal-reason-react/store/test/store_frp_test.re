open Store;

type state = {count: int};
type action =
  | Inc
  | Blocked;
type store = {state: state};
type row = {id: string};

let state = {count: 0};

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
    ],
  );
