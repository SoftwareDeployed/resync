open Melange_json.Primitives;

[@deriving json]
type state = Model.t;

type subscription = RealtimeSubscription.t;

type send_prompt_payload = {
  thread_id: string,
  prompt: string,
};

type action =
  | SendPrompt(send_prompt_payload)
  | SetInput(string)
  | AppendToken(string)
  | FinishResponse
  | SetError(string)
  | SelectThread(string);

type store = {
  state: state,
};

let emptyState: state = {
  threads: [||],
  current_thread_id: None,
  messages: [||],
  input: "",
  updated_at: 0.0,
};

let scopeKeyOfState = (state: state) =>
  switch (state.current_thread_id) {
  | Some(id) => id
  | None => "default"
  };

let timestampOfState = (state: state) => state.updated_at;

let setTimestamp = (~state: state, ~timestamp: float) =>
  {...state, updated_at: timestamp};

let action_to_json = action =>
  switch (action) {
  | SendPrompt(payload) =>
    StoreJson.parse(
      "{\"kind\":\"send_prompt\",\"payload\":{\"thread_id\":"
      ++ string_to_json(payload.thread_id)->Melange_json.to_string
      ++ ",\"prompt\":"
      ++ string_to_json(payload.prompt)->Melange_json.to_string
      ++ "}}"
    )
  | SetInput(text) =>
    StoreJson.parse(
      "{\"kind\":\"set_input\",\"payload\":{\"text\":"
      ++ string_to_json(text)->Melange_json.to_string
      ++ "}}"
    )
  | AppendToken(text) =>
    StoreJson.parse(
      "{\"kind\":\"append_token\",\"payload\":{\"text\":"
      ++ string_to_json(text)->Melange_json.to_string
      ++ "}}"
    )
  | FinishResponse =>
    StoreJson.parse("{\"kind\":\"finish_response\",\"payload\":{}}")
  | SetError(message) =>
    StoreJson.parse(
      "{\"kind\":\"set_error\",\"payload\":{\"message\":"
      ++ string_to_json(message)->Melange_json.to_string
      ++ "}}"
    )
  | SelectThread(thread_id) =>
    StoreJson.parse(
      "{\"kind\":\"select_thread\",\"payload\":{\"thread_id\":"
      ++ string_to_json(thread_id)->Melange_json.to_string
      ++ "}}"
    )
  };

let action_of_json = json => {
  let kind =
    StoreJson.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
  let payload =
    StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value => value);
  switch (kind) {
  | "send_prompt" =>
    SendPrompt({
      thread_id:
        StoreJson.requiredField(~json=payload, ~fieldName="thread_id", ~decode=string_of_json),
      prompt:
        StoreJson.requiredField(~json=payload, ~fieldName="prompt", ~decode=string_of_json),
    })
  | "set_input" =>
    SetInput(
      StoreJson.requiredField(~json=payload, ~fieldName="text", ~decode=string_of_json),
    )
  | "append_token" =>
    AppendToken(
      StoreJson.requiredField(~json=payload, ~fieldName="text", ~decode=string_of_json),
    )
  | "finish_response" => FinishResponse
  | "set_error" =>
    SetError(
      StoreJson.requiredField(~json=payload, ~fieldName="message", ~decode=string_of_json),
    )
  | "select_thread" =>
    SelectThread(
      StoreJson.requiredField(~json=payload, ~fieldName="thread_id", ~decode=string_of_json),
    )
  | _ => SetInput("")
  };
};

let reduce = (~state: state, ~action: action) => {
  let updatedAt = Js.Date.now();
  let withTimestamp = nextState => setTimestamp(~state=nextState, ~timestamp=updatedAt);
  switch (action) {
  | SendPrompt(payload) =>
    withTimestamp({
      ...state,
      messages:
        Js.Array.concat(
          ~other=[|
            {
              Model.Message.id: "local-" ++ string_of_float(updatedAt),
              thread_id: payload.thread_id,
              role: "user",
              content: payload.prompt,
            },
          |],
          state.messages,
        ),
      input: "",
    })
  | SetInput(text) =>
    {...state, input: text}
  | AppendToken(token) =>
    let messages = state.messages;
    let len = Array.length(messages);
    if (len == 0) {
      withTimestamp({
        ...state,
        messages: [|
          {
            Model.Message.id: "local-" ++ string_of_float(updatedAt),
            thread_id:
              switch (state.current_thread_id) {
              | Some(id) => id
              | None => ""
              },
            role: "assistant",
            content: token,
          },
        |],
      });
    } else {
      let lastIndex = len - 1;
      let lastMessage = messages[lastIndex];
      if (lastMessage.role == "assistant") {
        let prefix = Js.Array.slice(~start=0, ~end_=lastIndex, messages);
        withTimestamp({
          ...state,
          messages:
            Js.Array.concat(
              ~other=[|
                {...lastMessage, content: lastMessage.content ++ token},
              |],
              prefix,
            ),
        });
      } else {
        withTimestamp({
          ...state,
          messages:
            Js.Array.concat(
              ~other=[|
                {
                  Model.Message.id: "local-" ++ string_of_float(updatedAt),
                  thread_id:
                    switch (state.current_thread_id) {
                    | Some(id) => id
                    | None => ""
                    },
                  role: "assistant",
                  content: token,
                },
              |],
              messages,
            ),
        });
      };
    }
  | FinishResponse =>
    withTimestamp(state)
  | SetError(message) =>
    withTimestamp({
      ...state,
      messages:
        Js.Array.concat(
          ~other=[|
            {
              Model.Message.id: "local-error-" ++ string_of_float(updatedAt),
              thread_id:
                switch (state.current_thread_id) {
                | Some(id) => id
                | None => ""
                },
              role: "assistant",
              content: "Error: " ++ message,
            },
          |],
          state.messages,
        ),
    })
  | SelectThread(thread_id) =>
    withTimestamp({...state, current_thread_id: Some(thread_id), input: ""})
  };
};

let makeStore = (~state, ~derive=?, ()) => {
  let _ = derive;
  let store: store = {state: state};
  store;
};

[@platform js]
let onActionError = message => Js.log("Action error: " ++ message);

[@platform native]
let onActionError = _message => ();

type patch = unit;

let decodePatch = _json => None;

let updateOfPatch = (_patch, state) => state;

module StoreDef =
  StoreBuilder.Synced.Define({
    type nonrec state = state;
    type nonrec action = action;
    type nonrec store = store;
    type nonrec subscription = subscription;
    type nonrec patch = patch;

    let base: StoreBuilder.Synced.baseConfig(state, action, store, subscription) = {
      storeName: "llm-chat",
      emptyState,
      reduce,
      state_of_json,
      state_to_json,
      action_of_json,
      action_to_json,
      makeStore,
      scopeKeyOfState: state => scopeKeyOfState(state),
      timestampOfState: state => timestampOfState(state),
      setTimestamp,
      transport: {
        subscriptionOfState: (state: state): option(subscription) =>
          switch (state.current_thread_id) {
          | Some(id) => Some(RealtimeSubscription.thread(id))
          | None => None
          },
        encodeSubscription: RealtimeSubscription.encode,
        eventUrl: Constants.event_url,
        baseUrl: Constants.base_url,
      },
      stateElementId: Some("initial-store"),
      hooks:
        Some({
          StoreBuilder.Sync.onActionError: Some(onActionError),
          onActionAck: None,
          onCustom: None,
          onMedia: None,
          onError: None,
          onOpen: None,
          onConnectionHandle: None,
        }),
    };

    let strategy: StoreBuilder.Sync.customStrategy(state, patch) =
      StoreBuilder.Sync.custom(
        ~decodePatch,
        ~updateOfPatch,
      );
  });

include (
  StoreDef:
    StoreBuilder.Runtime.Exports
      with type state := state
      and type action := action
      and type t := store
);

type t = store;

module Context = StoreDef.Context;
