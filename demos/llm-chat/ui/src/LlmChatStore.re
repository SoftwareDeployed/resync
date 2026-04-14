open Melange_json.Primitives;

[@deriving json]
type state = Model.t;

type subscription = RealtimeSubscription.t;

type send_prompt_payload = {
  thread_id: string,
  prompt: string,
};

type stream_event =
  | StreamStarted(string)
  | TokenReceived(string, string)
  | StreamComplete(string);

type streaming_state = {
  activeStreams: Belt.Map.String.t(string),
};

let emptyStreamingState = {activeStreams: Belt.Map.String.empty};

type create_thread_payload = {
  id: string,
  title: string,
};

type action =
  | SendPrompt(send_prompt_payload)
  | CreateNewThread(create_thread_payload)
  | SetInput(string)
  | SetError(string)
  | SelectThread(string)
  | DeleteThread(string);

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
  | CreateNewThread(payload) =>
    StoreJson.parse(
      "{\"kind\":\"create_new_thread\",\"payload\":{\"id\":"
      ++ string_to_json(payload.id)->Melange_json.to_string
      ++ ",\"title\":"
      ++ string_to_json(payload.title)->Melange_json.to_string
      ++ "}}"
    )
  | SetInput(text) =>
    StoreJson.parse(
      "{\"kind\":\"set_input\",\"payload\":{\"text\":"
      ++ string_to_json(text)->Melange_json.to_string
      ++ "}}"
    )
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
  | DeleteThread(thread_id) =>
    StoreJson.parse(
      "{\"kind\":\"delete_thread\",\"payload\":{\"thread_id\":"
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
  | "set_error" =>
    SetError(
      StoreJson.requiredField(~json=payload, ~fieldName="message", ~decode=string_of_json),
    )
  | "create_new_thread" =>
    CreateNewThread({
      id: StoreJson.requiredField(~json=payload, ~fieldName="id", ~decode=string_of_json),
      title: StoreJson.requiredField(~json=payload, ~fieldName="title", ~decode=string_of_json),
    })
  | "select_thread" =>
    SelectThread(
      StoreJson.requiredField(~json=payload, ~fieldName="thread_id", ~decode=string_of_json),
    )
  | "delete_thread" =>
    DeleteThread(
      StoreJson.requiredField(~json=payload, ~fieldName="thread_id", ~decode=string_of_json),
    )
  | _ => SetInput("")
  };
};

let reduce = (~state: state, ~action: action) => {
  let updatedAt = Js.Date.now();
  let withTimestamp = nextState => setTimestamp(~state=nextState, ~timestamp=updatedAt);
  switch (action) {
  | CreateNewThread(payload) =>
    let alreadyExists =
      state.threads->Js.Array.some(~f=(t: Model.Thread.t) => t.id == payload.id);
    if (alreadyExists) {
      withTimestamp({
        ...state,
        current_thread_id: Some(payload.id),
        messages: [||],
        input: "",
      });
    } else {
      let newThread: Model.Thread.t = {
        id: payload.id,
        title: payload.title,
        updated_at: updatedAt,
      };
      let newThreads =
        Js.Array.concat(~other=state.threads, [|newThread|]);
      withTimestamp({
        ...state,
        threads: newThreads,
        current_thread_id: Some(payload.id),
        messages: [||],
        input: "",
      });
    };
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
    switch (state.current_thread_id) {
    | Some(id) when id == thread_id => state
    | _ => withTimestamp({...state, current_thread_id: Some(thread_id), messages: [||], input: ""})
    }
  | DeleteThread(thread_id) =>
    let remainingThreads =
      state.threads
      ->Js.Array.filter(~f=(t: Model.Thread.t) => t.id != thread_id);
    let nextThreadId =
      switch (Array.length(remainingThreads) > 0) {
      | true => Some(remainingThreads[0].id)
      | false => None
      };
    withTimestamp({
      ...state,
      threads: remainingThreads,
      current_thread_id: nextThreadId,
      messages:
        switch (state.current_thread_id) {
        | Some(current) when current == thread_id => [||]
        | _ => state.messages
        },
      input: "",
    })
  };
};

let makeStore = (~state, ~derive=?, ()) => {
  let _ = derive;
  {state: state};
};

[@platform js]
let onActionError = message => Js.log("Action error: " ++ message);

[@platform native]
let onActionError = _message => ();

let decodeStreamEvent = (json) =>
  switch (StoreJson.optionalField(~json, ~fieldName="event", ~decode=Melange_json.Primitives.string_of_json)) {
  | Some("stream_started") =>
    Some(StreamStarted(
      StoreJson.requiredField(~json, ~fieldName="message_id", ~decode=Melange_json.Primitives.string_of_json),
    ))
  | Some("token_received") =>
    Some(TokenReceived(
      StoreJson.requiredField(~json, ~fieldName="message_id", ~decode=Melange_json.Primitives.string_of_json),
      StoreJson.requiredField(~json, ~fieldName="token", ~decode=Melange_json.Primitives.string_of_json),
    ))
  | Some("stream_complete") =>
    Some(StreamComplete(
      StoreJson.requiredField(~json, ~fieldName="message_id", ~decode=Melange_json.Primitives.string_of_json),
    ))
  | _ => None
  };

let reduceStream = (streaming, event) =>
  switch (event) {
  | StreamStarted(id) =>
    {activeStreams: streaming.activeStreams->Belt.Map.String.set(id, "")}
  | TokenReceived(id, token) =>
    let current = streaming.activeStreams->Belt.Map.String.get(id)->Belt.Option.getWithDefault("");
    {activeStreams: streaming.activeStreams->Belt.Map.String.set(id, current ++ token)}
  | StreamComplete(id) =>
    {activeStreams: streaming.activeStreams->Belt.Map.String.remove(id)}
  };

type patch =
  | ThreadDeleted(string)
  | ThreadUpserted(Model.Thread.t)
  | MessageUpserted(Model.Message.t)
  | MessageDeleted(string);

let decodePatch = (json: StoreJson.json) => {
  let table =
    switch (StoreJson.field(json, "table")) {
    | Some(t) => Melange_json.Primitives.string_of_json(t)
    | None => ""
    };
  let action =
    switch (StoreJson.field(json, "action")) {
    | Some(a) => Melange_json.Primitives.string_of_json(a)
    | None => ""
    };
  switch (table, action) {
  | ("threads", "DELETE") =>
    switch (StoreJson.field(json, "id")) {
    | Some(idJson) =>
      Some(ThreadDeleted(Melange_json.Primitives.string_of_json(idJson)))
    | None => None
    }
  | ("threads", "INSERT")
  | ("threads", "UPDATE") =>
    switch (StoreJson.field(json, "data")) {
    | Some(dataJson) =>
      Some(ThreadUpserted(Model.Thread.of_json(dataJson)))
    | None => None
    }
  | ("messages", "INSERT")
  | ("messages", "UPDATE") =>
    switch (StoreJson.field(json, "data")) {
    | Some(dataJson) =>
      Some(MessageUpserted(Model.Message.of_json(dataJson)))
    | None => None
    }
  | ("messages", "DELETE") =>
    switch (StoreJson.field(json, "id")) {
    | Some(idJson) =>
      Some(MessageDeleted(Melange_json.Primitives.string_of_json(idJson)))
    | None => None
    }
  | _ => None
  };
};

let updateOfPatch = (patch: patch, state: state): state =>
  switch (patch) {
  | ThreadDeleted(threadId) =>
    let remainingThreads =
      state.threads
      ->Js.Array.filter(~f=(t: Model.Thread.t) => t.id != threadId);
    let nextThreadId =
      switch (Array.length(remainingThreads) > 0) {
      | true => Some(remainingThreads[0].id)
      | false => None
      };
    {
      ...state,
      threads: remainingThreads,
      current_thread_id: nextThreadId,
      messages:
        switch (state.current_thread_id) {
        | Some(current) when current == threadId => [||]
        | _ => state.messages
        },
      input: "",
    };
  | ThreadUpserted(thread) =>
    let alreadyExists =
      state.threads->Js.Array.some(~f=(t: Model.Thread.t) => t.id == thread.id);
    if (alreadyExists) {
      let updatedThreads =
        state.threads->Js.Array.map(~f=(t: Model.Thread.t) =>
          t.id == thread.id ? thread : t
        );
      {...state, threads: updatedThreads};
    } else {
      let newThreads =
        Js.Array.concat(~other=[|thread|], state.threads);
      let sortedThreads =
        newThreads
        |> Array.to_list
        |> List.sort((a: Model.Thread.t, b: Model.Thread.t) =>
             compare(b.updated_at, a.updated_at)
           )
        |> Array.of_list;
      {...state, threads: sortedThreads};
    };
  | MessageUpserted(msg) =>
    switch (state.current_thread_id) {
    | Some(current_thread_id) when current_thread_id == msg.thread_id =>
      let filteredMessages =
        state.messages
        ->Js.Array.filter(~f=(m: Model.Message.t) => m.id != msg.id);
      {
        ...state,
        messages:
          Js.Array.concat(
            ~other=[|msg|],
            filteredMessages,
          ),
      };
    | _ => state
    }
  | MessageDeleted(id) =>
    {
      ...state,
      messages:
        state.messages
        ->Js.Array.filter(~f=(m: Model.Message.t) => m.id != id),
    }
  };

let reconcilePatch = (patch, streaming) =>
  switch (patch) {
  | MessageUpserted(msg) =>
    {activeStreams: streaming.activeStreams->Belt.Map.String.remove(msg.id)}
  | _ => streaming
  };

let guardTree =
  StoreBuilder.GuardTree.whenTrue(
    ~condition=(state: state) =>
      switch (state.current_thread_id) {
      | Some(_) => true
      | None => false
      },
    ~then_=StoreBuilder.GuardTree.acceptAll,
    ~else_=
      StoreBuilder.GuardTree.denyIf(
        ~predicate=(action: action) =>
          switch (action) {
          | SendPrompt(_) | DeleteThread(_) | SelectThread(_) => true
          | _ => false
          },
        ~reason="No active thread",
        (),
      ),
    (),
  );

module StoreDef =
  (val StoreBuilder.buildSynced(
    StoreBuilder.make()
    |> StoreBuilder.withSchema({
         emptyState,
         reduce,
         makeStore,
       })
    |> StoreBuilder.withGuardTree(~guardTree)
    |> StoreBuilder.withJson(~state_of_json, ~state_to_json, ~action_of_json, ~action_to_json)
    |> StoreBuilder.withSync(
         ~storeName = "llm-chat",
         ~scopeKeyOfState = state => scopeKeyOfState(state),
         ~timestampOfState = state => timestampOfState(state),
         ~setTimestamp,
         ~decodePatch,
         ~updateOfPatch,
         ~transport = {
           subscriptionOfState: (state: state): option(subscription) =>
             switch (state.current_thread_id) {
             | Some(id) => Some(RealtimeSubscription.thread(id))
             | None => None
             },
           encodeSubscription: RealtimeSubscription.encode,
           eventUrl: Constants.event_url,
           baseUrl: Constants.base_url,
         },
         ~streams=Some({
           decodeStreamEvent,
           emptyStreamingState,
           reduceStream,
           reconcilePatch,
         }),
  ~hooks={
    StoreBuilder.Sync.onActionError: Some(onActionError),
    onActionAck: None,
    onCustom: None,
    onMedia: None,
    onError: None,
    onOpen: None,
    onMultiplexedHandle: None,
  },
         ~stateElementId=Some("initial-store"),
         (),
       ),
  ));

include (
  StoreDef:
    StoreBuilder.Runtime.Exports
      with type state := state
      and type action := action
      and type t := store
      and type stream_event := stream_event
      and type streaming_state := streaming_state
);

type t = store;

module Context = StoreDef.Context;
