open Melange_json.Primitives;

[@deriving json]
type state = Model.t;

type subscription = RealtimeSubscription.t;

type send_prompt_payload = {
  message_id: string,
  thread_id: string,
  prompt: string,
};

type stream_event =
  | StreamStarted(string, string)
  | TokenReceived(string, string, string)
  | StreamComplete(string, string)
  | StreamError(string, string, string);

type stream_error = {
  thread_id: string,
  message: string,
};

type streaming_state = {
  activeStreams: Belt.Map.String.t(string),
  currentStreamId: option(string),
  currentThreadId: option(string),
  isStreaming: bool,
  streamError: option(stream_error),
};

let emptyStreamingState = {
  activeStreams: Belt.Map.String.empty,
  currentStreamId: None,
  currentThreadId: None,
  isStreaming: false,
  streamError: None,
};

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

let actionJsonWithPayload = (~kind, ~payload) =>
  StoreJson.Object.make(dict => {
    StoreJson.Object.setString(dict, "kind", kind);
    StoreJson.Object.setJson(dict, "payload", payload);
  });

let actionJson = (~kind, ~fill) =>
  actionJsonWithPayload(~kind, ~payload=StoreJson.Object.make(fill));

let sendPromptPayloadJson = (payload: send_prompt_payload) =>
  StoreJson.Object.make(dict => {
    StoreJson.Object.setString(dict, "message_id", payload.message_id);
    StoreJson.Object.setString(dict, "thread_id", payload.thread_id);
    StoreJson.Object.setString(dict, "prompt", payload.prompt);
  });

let createThreadPayloadJson = (payload: create_thread_payload) =>
  StoreJson.Object.make(dict => {
    StoreJson.Object.setString(dict, "id", payload.id);
    StoreJson.Object.setString(dict, "title", payload.title);
  });

let threadIdPayloadJson = (thread_id: string) =>
  StoreJson.Object.make(dict =>
    StoreJson.Object.setString(dict, "thread_id", thread_id)
  );

let action_to_json = action =>
  switch (action) {
  | SendPrompt(payload) =>
    actionJsonWithPayload(
      ~kind="send_prompt",
      ~payload=sendPromptPayloadJson(payload),
    )
  | CreateNewThread(payload) =>
    actionJsonWithPayload(
      ~kind="create_new_thread",
      ~payload=createThreadPayloadJson(payload),
    )
  | SetInput(text) =>
    actionJson(
      ~kind="set_input",
      ~fill=dict => StoreJson.Object.setString(dict, "text", text),
    )
  | SetError(message) =>
    actionJson(
      ~kind="set_error",
      ~fill=dict => StoreJson.Object.setString(dict, "message", message),
    )
  | SelectThread(thread_id) =>
    actionJsonWithPayload(
      ~kind="select_thread",
      ~payload=threadIdPayloadJson(thread_id),
    )
  | DeleteThread(thread_id) =>
    actionJsonWithPayload(
      ~kind="delete_thread",
      ~payload=threadIdPayloadJson(thread_id),
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
      message_id:
        switch (
          StoreJson.optionalField(~json=payload, ~fieldName="message_id", ~decode=string_of_json)
        ) {
        | Some(id) => id
        | None => UUID.make()
        },
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

module Mutations = {
  module SendPrompt = {
    type params = send_prompt_payload;
    type nonrec action = action;
    let toAction = params => SendPrompt(params);
  };

  module CreateNewThread = {
    type params = create_thread_payload;
    type nonrec action = action;
    let toAction = params => CreateNewThread(params);
  };

  module SelectThread = {
    type params = string;
    type nonrec action = action;
    let toAction = thread_id => SelectThread(thread_id);
  };

  module DeleteThread = {
    type params = string;
    type nonrec action = action;
    let toAction = thread_id => DeleteThread(thread_id);
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
        [|newThread|]->Js.Array.concat(~other=state.threads);
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
        state.messages->Js.Array.concat(
          ~other=[|
            {
              Model.Message.id: payload.message_id,
              thread_id: payload.thread_id,
              role: "user",
              content: payload.prompt,
            },
          |],
        ),
      input: "",
    })
  | SetInput(text) =>
    {...state, input: text}
  | SetError(message) =>
    withTimestamp({
      ...state,
      messages:
        state.messages->Js.Array.concat(
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

[@platform js]
let onActionError = message => Js.log("Action error: " ++ message);

[@platform native]
let onActionError = _message => ();

let decodeStreamEvent = (json) =>
  switch (StoreJson.optionalField(~json, ~fieldName="event", ~decode=Melange_json.Primitives.string_of_json)) {
  | Some("stream_started") =>
    Some(StreamStarted(
      StoreJson.requiredField(~json, ~fieldName="thread_id", ~decode=Melange_json.Primitives.string_of_json),
      StoreJson.requiredField(~json, ~fieldName="message_id", ~decode=Melange_json.Primitives.string_of_json),
    ))
  | Some("token_received") =>
    Some(TokenReceived(
      StoreJson.requiredField(~json, ~fieldName="thread_id", ~decode=Melange_json.Primitives.string_of_json),
      StoreJson.requiredField(~json, ~fieldName="message_id", ~decode=Melange_json.Primitives.string_of_json),
      StoreJson.requiredField(~json, ~fieldName="token", ~decode=Melange_json.Primitives.string_of_json),
    ))
  | Some("stream_complete") =>
    Some(StreamComplete(
      StoreJson.requiredField(~json, ~fieldName="thread_id", ~decode=Melange_json.Primitives.string_of_json),
      StoreJson.requiredField(~json, ~fieldName="message_id", ~decode=Melange_json.Primitives.string_of_json),
    ))
  | Some("stream_error") =>
    Some(StreamError(
      StoreJson.requiredField(~json, ~fieldName="thread_id", ~decode=Melange_json.Primitives.string_of_json),
      StoreJson.requiredField(~json, ~fieldName="message_id", ~decode=Melange_json.Primitives.string_of_json),
      StoreJson.requiredField(~json, ~fieldName="error", ~decode=Melange_json.Primitives.string_of_json),
    ))
  | _ => None
  };

let reduceStream = (streaming, event) =>
  switch (event) {
  | StreamStarted(threadId, id) =>
    {
      activeStreams: streaming.activeStreams->Belt.Map.String.set(id, ""),
      currentStreamId: Some(id),
      currentThreadId: Some(threadId),
      isStreaming: true,
      streamError: None,
    }
  | TokenReceived(threadId, id, token) =>
    let current = streaming.activeStreams->Belt.Map.String.get(id)->Belt.Option.getWithDefault("");
    {
      activeStreams: streaming.activeStreams->Belt.Map.String.set(id, current ++ token),
      currentStreamId: Some(id),
      currentThreadId: Some(threadId),
      isStreaming: true,
      streamError: None,
    }
  | StreamComplete(threadId, id) =>
    {
      ...streaming,
      currentStreamId: Some(id),
      currentThreadId: Some(threadId),
      isStreaming: false,
      streamError: None,
    }
  | StreamError(threadId, id, message) =>
    {
      activeStreams: streaming.activeStreams->Belt.Map.String.remove(id),
      currentStreamId: None,
      currentThreadId: Some(threadId),
      isStreaming: false,
      streamError: Some({thread_id: threadId, message}),
    }
  };

type patch =
  | ThreadsPatch(StoreCrud.patch(Model.Thread.t))
  | MessagesPatch(StoreCrud.patch(Model.Message.t));

let decodeThreadsPatch =
  StoreCrud.decodePatch(
    ~table=RealtimeSchema.table_name("threads"),
    ~decodeRow=Model.Thread.of_json,
    (),
  );

let decodeMessagesPatch =
  StoreCrud.decodePatch(
    ~table=RealtimeSchema.table_name("messages"),
    ~decodeRow=Model.Message.of_json,
    (),
  );

let decodePatch =
  StorePatch.compose([|
    json =>
      switch (decodeThreadsPatch(json)) {
      | Some(patch) => Some(ThreadsPatch(patch))
      | None => None
      },
    json =>
      switch (decodeMessagesPatch(json)) {
      | Some(patch) => Some(MessagesPatch(patch))
      | None => None
      },
  |]);

let insertThreadByUpdatedAt = (threads: array(Model.Thread.t), thread: Model.Thread.t) => {
  let rec insertionIndex = index =>
    if (index >= Array.length(threads)) {
      index;
    } else if (thread.updated_at >= threads[index].updated_at) {
      index;
    } else {
      insertionIndex(index + 1);
    };

  let index = insertionIndex(0);
  let before = Js.Array.slice(~start=0, ~end_=index, threads);
  let after = Js.Array.slice(~start=index, ~end_=Array.length(threads), threads);
  let withThread = before->Js.Array.concat(~other=[|thread|]);
  withThread->Js.Array.concat(~other=after);
};

let sortThreadsByUpdatedAt = (threads: array(Model.Thread.t)) => {
  let rec loop = (index, sorted) =>
    if (index >= Array.length(threads)) {
      sorted;
    } else {
      loop(index + 1, insertThreadByUpdatedAt(sorted, threads[index]));
    };

  loop(0, [||]);
};

let updateOfPatch = (patch: patch, state: state): state =>
  switch (patch) {
  | ThreadsPatch(StoreCrud.Delete(threadId)) =>
    let remainingThreads =
      StoreCrud.remove(~getId=(t: Model.Thread.t) => t.id, state.threads, threadId);
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
  | ThreadsPatch(StoreCrud.Upsert(thread)) =>
    let threads =
      StoreCrud.upsert(~getId=(t: Model.Thread.t) => t.id, state.threads, thread)
      |> sortThreadsByUpdatedAt;
    {...state, threads};
  | MessagesPatch(StoreCrud.Upsert(msg)) =>
    switch (state.current_thread_id) {
    | Some(current_thread_id) when current_thread_id == msg.thread_id =>
      {
        ...state,
        messages: StoreCrud.upsert(~getId=(m: Model.Message.t) => m.id, state.messages, msg),
      };
    | _ => state
    }
  | MessagesPatch(StoreCrud.Delete(id)) =>
    {
      ...state,
      messages: StoreCrud.remove(~getId=(m: Model.Message.t) => m.id, state.messages, id),
    }
  };

let reconcilePatch = (patch, streaming) =>
  switch (patch) {
  | MessagesPatch(StoreCrud.Upsert(msg)) =>
    let isCurrent =
      switch (streaming.currentStreamId) {
      | Some(id) => id == msg.id
      | None => false
      };
    {
      activeStreams: streaming.activeStreams->Belt.Map.String.remove(msg.id),
      currentStreamId: isCurrent ? None : streaming.currentStreamId,
      currentThreadId: isCurrent ? None : streaming.currentThreadId,
      isStreaming: isCurrent ? false : streaming.isStreaming,
      streamError: None,
    }
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
         makeStore: (~state, ~derive=?, ()) => {
           let _ = derive;
           {state: state};
         },
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
         ~hooks=StoreBuilder.Sync.hooks(~onActionError=onActionError, ()),
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
module Hooks = StoreDef.Hooks;
