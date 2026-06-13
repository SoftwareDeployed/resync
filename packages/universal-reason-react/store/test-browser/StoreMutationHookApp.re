[@mel.scope "document"]
external getElementById: string => Js.Nullable.t('a) = "getElementById";

external setTimeout: (unit => unit, int) => unit = "setTimeout";

type emptyProps = Js.t({.});
[@mel.obj]
external makeEmptyProps: unit => emptyProps = "";

type scopedState = {
  count: int,
  updatedAt: float,
};

type scopedAction =
  | ScopedAdd(int)
  | ScopedBlocked;

type scopedStore = {
  state: scopedState,
};

let scopedStateToJson = (state: scopedState) =>
  StoreJson.Object.make(dict => {
    StoreJson.Object.setInt(dict, "count", state.count);
    StoreJson.Object.setFloat(dict, "updated_at", state.updatedAt);
  });

let scopedStateOfJson = json => {
  count:
    StoreJson.requiredField(
      ~json,
      ~fieldName="count",
      ~decode=Melange_json.Primitives.int_of_json,
    ),
  updatedAt:
    StoreJson.requiredField(
      ~json,
      ~fieldName="updated_at",
      ~decode=Melange_json.Primitives.float_of_json,
    ),
};

let scopedActionToJson = action =>
  StoreJson.Object.make(dict =>
    switch (action) {
    | ScopedAdd(amount) =>
      StoreJson.Object.setString(dict, "kind", "add");
      StoreJson.Object.setInt(dict, "amount", amount);
    | ScopedBlocked => StoreJson.Object.setString(dict, "kind", "blocked")
    }
  );

let scopedActionOfJson = json => {
  let kind =
    StoreJson.requiredField(
      ~json,
      ~fieldName="kind",
      ~decode=Melange_json.Primitives.string_of_json,
    );
  switch (kind) {
  | "add" =>
    ScopedAdd(
      StoreJson.requiredField(
        ~json,
        ~fieldName="amount",
        ~decode=Melange_json.Primitives.int_of_json,
      ),
    )
  | _ => ScopedBlocked
  };
};

let scopedGuardTree =
  StoreBuilder.GuardTree.denyIf(
    ~predicate=action =>
      switch (action) {
      | ScopedBlocked => true
      | ScopedAdd(_) => false
      },
    ~reason="blocked",
    (),
  );

module ScopedStore = {
  module Queries = {
    module Skipped = {
      type params = string;
      type row = string;

      let channel = param => "store-scoped-query:" ++ param;
      let paramsHash = param => param;
      let decodeRow = Melange_json.Primitives.string_of_json;
      let row_to_json = Melange_json.Primitives.string_to_json;
    };
  };

  module Mutations = {
    module Add = {
      type params = int;
      type nonrec action = scopedAction;
      let toAction = amount => ScopedAdd(amount);
    };

    module Blocked = {
      type params = unit;
      type nonrec action = scopedAction;
      let toAction = () => ScopedBlocked;
    };
  };

  module StoreDef = Store.Frp.Local.Build({
    type state = scopedState;
    type action = scopedAction;
    type store = scopedStore;

    let config =
      Store.Frp.Local.withCache(
        `None,
        Store.Frp.Local.make(
          ~guardTree=scopedGuardTree,
          {
            storeName: "store-mutation-hook-browser-test",
            emptyState: {count: 0, updatedAt: 0.0},
            reduce: (~state, ~action) =>
              switch (action) {
              | ScopedAdd(amount) => {
                  count: state.count + amount,
                  updatedAt: Js.Date.now(),
                }
              | ScopedBlocked => state
              },
            state_of_json: scopedStateOfJson,
            state_to_json: scopedStateToJson,
            action_of_json: scopedActionOfJson,
            action_to_json: scopedActionToJson,
            makeStore: (
              ~state: scopedState,
              ~derive: option(Tilia.Core.deriver(scopedStore))=?,
              (),
            ) => {
              state:
                StoreBuilder.current(
                  ~derive?,
                  ~client=state,
                  ~server=() => state,
                  (),
                ),
            },
            scopeKeyOfState: _state => "default",
            timestampOfState: state => state.updatedAt,
            stateElementId: None,
          },
        ),
      );
  });

  include (
    StoreDef:
      StoreBuilder.Runtime.Exports
        with type state := scopedState
        and type action := scopedAction
        and type t := scopedStore
  );

  module Context = StoreDef.Context;
};

module TestMutation = {
  type params = string;
};

let delayedResolve = () =>
  Js.Promise.make((~resolve, ~reject as _) => {
    setTimeout(() => {
      let unitValue = ();
      resolve(. unitValue);
    }, 120);
  });

let delayedReject = message =>
  Js.Promise.make((~resolve as _, ~reject) => {
    setTimeout(() => reject(. Failure(message)), 120);
  });

module StoreScopedProbe = {
  [@react.component]
  let make = () => {
    let store = ScopedStore.Context.useStore();
    let queryResult =
      ScopedStore.Hooks.useQuery(
        (module ScopedStore.Queries.Skipped),
        "skip",
        ~skip=true,
        (),
      );
    let storeFromQuery =
      ScopedStore.Hooks.useQueryStore(
        (module ScopedStore.Queries.Skipped),
        "skip",
        ~skip=true,
        (),
      );
    let add = ScopedStore.Hooks.useMutation((module ScopedStore.Mutations.Add), ());
    let addResult =
      ScopedStore.Hooks.useMutationResult((module ScopedStore.Mutations.Add), ());
    let blockedResult =
      ScopedStore.Hooks.useMutationResult(
        (module ScopedStore.Mutations.Blocked),
        (),
      );
    let (renderCount, setRenderCount) = React.useState(() => 0);
    let (fnCompletedCount, setFnCompletedCount) = React.useState(() => 0);
    let (resultCompletedCount, setResultCompletedCount) =
      React.useState(() => 0);
    let (fnStableLabel, setFnStableLabel) = React.useState(() => "initial");
    let (resultStableLabel, setResultStableLabel) =
      React.useState(() => "initial");
    let previousFnRef = React.useRef(None);
    let previousResultRef = React.useRef(None);

    React.useEffect1(() => {
      let nextFnStableLabel =
        switch (previousFnRef.current) {
        | None => "initial"
        | Some(previousAdd) => previousAdd == add ? "yes" : "no"
        };
      previousFnRef.current = Some(add);
      setFnStableLabel(current =>
        current == nextFnStableLabel ? current : nextFnStableLabel
      );
      let nextResultStableLabel =
        switch (previousResultRef.current) {
        | None => "initial"
        | Some(previousMutate) =>
          previousMutate == addResult.mutate ? "yes" : "no"
        };
      previousResultRef.current = Some(addResult.mutate);
      setResultStableLabel(current =>
        current == nextResultStableLabel ? current : nextResultStableLabel
      );
      None;
    }, [|renderCount|]);

    let runFnSuccess = _event => {
      add(2)
      |> Js.Promise.then_(_ => {
           setFnCompletedCount(count => count + 1);
           Js.Promise.resolve();
         })
      |> ignore;
    };

    let runResultSuccess = _event => {
      addResult.mutate(3)
      |> Js.Promise.then_(_ => {
           setResultCompletedCount(count => count + 1);
           Js.Promise.resolve();
         })
      |> ignore;
    };

    let runBlocked = _event => {
      blockedResult.mutate(())
      |> Js.Promise.catch(_ => Js.Promise.resolve())
      |> ignore;
    };

    <section id="store-scoped-mutation-hook-app">
      <div id="scoped-mutation-count">
        {React.string(string_of_int(store.state.count))}
      </div>
      <div id="scoped-query-loading">
        {React.string(queryResult.loading ? "loading" : "not-loading")}
      </div>
      <div id="scoped-query-store-count">
        {React.string(string_of_int(storeFromQuery.state.count))}
      </div>
      <div id="scoped-mutation-fn-stable"> {React.string(fnStableLabel)} </div>
      <div id="scoped-mutation-result-stable">
        {React.string(resultStableLabel)}
      </div>
      <div id="scoped-mutation-fn-completed">
        {React.string(string_of_int(fnCompletedCount))}
      </div>
      <div id="scoped-mutation-result-completed">
        {React.string(string_of_int(resultCompletedCount))}
      </div>
      <div id="scoped-mutation-error">
        {React.string(
           switch (blockedResult.error) {
           | Some(error) => error
           | None => "none"
           },
         )}
      </div>
      <button
        id="scoped-mutation-rerender"
        type_="button"
        onClick={_event => setRenderCount(count => count + 1)}>
        {React.string("rerender store scoped")}
      </button>
      <button id="scoped-mutation-fn-success" type_="button" onClick=runFnSuccess>
        {React.string("store scoped function success")}
      </button>
      <button
        id="scoped-mutation-result-success"
        type_="button"
        onClick=runResultSuccess>
        {React.string("store scoped result success")}
      </button>
      <button id="scoped-mutation-blocked" type_="button" onClick=runBlocked>
        {React.string("store scoped blocked")}
      </button>
    </section>;
  };
};

[@react.component]
let make = () => {
  let (renderCount, setRenderCount) = React.useState(() => 0);
  let (callCount, setCallCount) = React.useState(() => 0);
  let (completedCount, setCompletedCount) = React.useState(() => 0);
  let (lastParam, setLastParam) = React.useState(() => "none");
  let (stableLabel, setStableLabel) = React.useState(() => "initial");
  let (fnCallCount, setFnCallCount) = React.useState(() => 0);
  let (fnCompletedCount, setFnCompletedCount) = React.useState(() => 0);
  let (fnLastParam, setFnLastParam) = React.useState(() => "none");
  let (fnStableLabel, setFnStableLabel) = React.useState(() => "initial");
  let previousMutateRef = React.useRef(None);
  let previousFnMutateRef = React.useRef(None);

  let mutation =
    Hooks.useMutationResult(
      (module TestMutation),
      ~onDispatch=params => {
        setCallCount(count => count + 1);
        setLastParam(_ => params);
        switch (params) {
        | "fail" => delayedReject("boom")
        | _ => delayedResolve()
        };
      },
      (),
    );

  let mutateFn =
    Hooks.useMutation(
      (module TestMutation),
      ~onDispatch=params => {
        setFnCallCount(count => count + 1);
        setFnLastParam(_ => params);
        delayedResolve();
      },
      (),
    );

  React.useEffect1(() => {
    let nextStableLabel =
      switch (previousMutateRef.current) {
      | None => "initial"
      | Some(previousMutate) => previousMutate == mutation.mutate ? "yes" : "no"
      };
    previousMutateRef.current = Some(mutation.mutate);
    setStableLabel(current => current == nextStableLabel ? current : nextStableLabel);
    let nextFnStableLabel =
      switch (previousFnMutateRef.current) {
      | None => "initial"
      | Some(previousMutate) => previousMutate == mutateFn ? "yes" : "no"
      };
    previousFnMutateRef.current = Some(mutateFn);
    setFnStableLabel(current => current == nextFnStableLabel ? current : nextFnStableLabel);
    None;
  }, [|renderCount|]);

  let runSuccess = _event => {
    mutation.mutate("ok")
    |> Js.Promise.then_(_ => {
         setCompletedCount(count => count + 1);
         Js.Promise.resolve();
       })
    |> ignore;
  };

  let runFailure = _event => {
    mutation.mutate("fail")
    |> Js.Promise.catch(_ => Js.Promise.resolve())
    |> ignore;
  };

  let runFnSuccess = _event => {
    mutateFn("fn-ok")
    |> Js.Promise.then_(_ => {
         setFnCompletedCount(count => count + 1);
         Js.Promise.resolve();
       })
    |> ignore;
  };

  <div id="mutation-hook-app">
    <div id="mutation-loading">
      {React.string(mutation.loading ? "loading" : "idle")}
    </div>
    <div id="mutation-error">
      {React.string(
         switch (mutation.error) {
         | Some(error) => error
         | None => "none"
         },
       )}
    </div>
    <div id="mutation-calls"> {React.string(string_of_int(callCount))} </div>
    <div id="mutation-completed">
      {React.string(string_of_int(completedCount))}
    </div>
    <div id="mutation-last-param"> {React.string(lastParam)} </div>
    <div id="mutation-stable"> {React.string(stableLabel)} </div>
    <div id="mutation-renders"> {React.string(string_of_int(renderCount))} </div>
    <div id="mutation-fn-calls"> {React.string(string_of_int(fnCallCount))} </div>
    <div id="mutation-fn-completed">
      {React.string(string_of_int(fnCompletedCount))}
    </div>
    <div id="mutation-fn-last-param"> {React.string(fnLastParam)} </div>
    <div id="mutation-fn-stable"> {React.string(fnStableLabel)} </div>
    <button
      id="mutation-rerender"
      type_="button"
      onClick={_event => setRenderCount(count => count + 1)}>
      {React.string("rerender")}
    </button>
    <button id="mutation-success" type_="button" onClick=runSuccess>
      {React.string("success")}
    </button>
    <button id="mutation-failure" type_="button" onClick=runFailure>
      {React.string("failure")}
    </button>
    <button id="mutation-fn-success" type_="button" onClick=runFnSuccess>
      {React.string("function success")}
    </button>
    <StoreScopedProbe />
  </div>;
};

let root =
  switch (getElementById("root")->Js.Nullable.toOption) {
  | Some(el) => el
  | None => failwith("Missing root element")
  };

let scopedStore = ScopedStore.hydrateStore();

let app =
  React.createElement(
    ScopedStore.Context.Provider.make,
    {
      "value": scopedStore,
      "children": React.createElement(make, makeEmptyProps()),
    },
  );

ReactDOM.Client.createRoot(root)->ReactDOM.Client.render(app);
