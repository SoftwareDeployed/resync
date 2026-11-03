[@melange_only]
module Style = {
  [%%raw "import \"./style.css\""];
  [%%raw "import \"streamdown/styles.css\""];
};

[@native_only]
module Style = {
  let _css = ();
};

open Tilia.React;

[@melange_only]
let handleInputChange = (setDraft, event) => {
  let value = React.Event.Form.target(event)##value;
  setDraft(_ => value);
};

[@native_only]
let handleInputChange = (_setDraft, _event) => ();

[@melange_only]
let handleSend =
    (
      store: LlmChatStore.t,
      sendPromptMutation: UseMutation.mutation_result(LlmChatStore.send_prompt_payload),
      prompt,
      setDraft,
    ) => {
  let threadId =
    switch (store.state.current_thread_id) {
    | Some(id) => id
    | None => ""
  };
  let trimmedPrompt = String.trim(prompt);
  if (String.length(trimmedPrompt) > 0 && String.length(threadId) > 0) {
    let messageId = UUID.make();
    let assistantMessageId = UUID.make();
    let _ =
      sendPromptMutation.mutate({
        message_id: messageId,
        assistant_message_id: assistantMessageId,
        thread_id: threadId,
        prompt: trimmedPrompt,
      });
    setDraft(_ => "");
  };
};

[@native_only]
let handleSend = (_store, _sendPromptMutation, _prompt, _setDraft) =>
  ();

let onThreadClick =
    (
      _router: UniversalRouter.routerApi,
      selectThread: string => Js.Promise.t(unit),
      threadId,
      _event,
    ) => {
  let _ = selectThread(threadId);
  ReasonReactRouter.push("/" ++ threadId);
};

let onNewChatClick =
    (
      createThread: LlmChatStore.create_thread_payload => Js.Promise.t(unit),
      router: UniversalRouter.routerApi,
      _event,
    ) => {
  let uuid = UUID.make();
  let _ =
    createThread({id: uuid, title: "New Chat"})
    |> Js.Promise.then_(_ => {
         router.push("/" ++ uuid);
         Js.Promise.resolve();
       })
    |> Js.Promise.catch(_ => Js.Promise.resolve());
  ();
};

let onDeleteThread =
    (
      _router: UniversalRouter.routerApi,
      deleteThread: string => Js.Promise.t(unit),
      threadId,
      event,
    ) => {
  React.Event.Mouse.stopPropagation(event);
  React.Event.Mouse.preventDefault(event);
  let _ = deleteThread(threadId);
};

[@melange_only]
let handleKeyDown =
    (
      store,
      sendPromptMutation: UseMutation.mutation_result(LlmChatStore.send_prompt_payload),
      draft,
      setDraft,
      event,
    ) => {
  let key = React.Event.Keyboard.key(event);
  let shift = React.Event.Keyboard.shiftKey(event);
  if (key == "Enter" && !shift) {
    React.Event.Keyboard.preventDefault(event);
    handleSend(store, sendPromptMutation, draft, setDraft);
  };
};

[@native_only]
let handleKeyDown = (_store, _sendPromptMutation, _draft, _setDraft, _event) =>
  ();

let hasCurrentStreamAssistantMessage = (~messages, streaming: LlmChatStore.streaming_state) =>
  switch (streaming.currentStreamId) {
  | Some(streamId) =>
    messages->Js.Array.some(~f=(message: Model.Message.t) =>
      message.role == "assistant" && message.id == streamId
    )
  | None => false
  };

let isStreamingForThread = (~currentThreadId, streaming: LlmChatStore.streaming_state) =>
  switch (streaming.currentThreadId) {
  | Some(threadId) => threadId == currentThreadId && streaming.isStreaming
  | None => false
  };

let streamingTextForThread = (~currentThreadId, streaming: LlmChatStore.streaming_state) => {
  let activeText =
    switch (streaming.currentThreadId, streaming.currentStreamId) {
    | (Some(threadId), Some(streamId)) when threadId == currentThreadId =>
      streaming.activeStreams
      ->Belt.Map.String.get(streamId)
      ->Belt.Option.getWithDefault("")
    | _ => ""
    };
  if (String.length(activeText) > 0) {
    activeText;
  } else {
    switch (streaming.streamError) {
    | Some({thread_id, message}) when thread_id == currentThreadId =>
      "Error: " ++ message
    | _ => ""
    };
  };
};

let currentStreamIdForThread = (~currentThreadId, streaming: LlmChatStore.streaming_state) =>
  switch (streaming.currentThreadId, streaming.currentStreamId) {
  | (Some(threadId), Some(streamId)) when threadId == currentThreadId =>
    Some(streamId)
  | _ => None
  };

let startsWith = (~prefix, value) => {
  let prefixLength = String.length(prefix);
  String.length(value) >= prefixLength
  && String.sub(value, 0, prefixLength) == prefix;
};

let activeStreamTextForMessage = (~streaming: LlmChatStore.streaming_state, ~messageId) =>
  switch (streaming.currentStreamId) {
  | Some(streamId) when streamId == messageId =>
    streaming.activeStreams
    ->Belt.Map.String.get(streamId)
    ->Belt.Option.getWithDefault("")
  | _ => ""
  };

let displayAssistantContent = (~message: Model.Message.t, ~streaming) => {
  let activeText = activeStreamTextForMessage(~streaming, ~messageId=message.id);
  if (String.length(activeText) == 0) {
    message.content;
  } else if (startsWith(~prefix=message.content, activeText)) {
    activeText;
  } else if (startsWith(~prefix=activeText, message.content)) {
    message.content;
  } else {
    message.content ++ activeText;
  };
};

let appendCurrentStreamMessage =
    (
      ~messages,
      ~currentThreadId,
      ~currentStreamId,
      ~hasCurrentStreamRow,
      ~streamingText,
      ~streamError: option(LlmChatStore.stream_error),
    ) =>
  switch (currentStreamId, streamError) {
  | (Some(streamId), _)
      when String.length(streamingText) > 0 && !hasCurrentStreamRow =>
    messages->Js.Array.concat(
      ~other=[|
        {
          Model.Message.id: streamId,
          thread_id: currentThreadId,
          role: "assistant",
          content: "",
        },
      |],
    )
  | (None, Some({thread_id, message}))
      when thread_id == currentThreadId && String.length(streamingText) > 0 =>
    messages->Js.Array.concat(
      ~other=[|
        {
          Model.Message.id: "stream-error-" ++ thread_id,
          thread_id: currentThreadId,
          role: "assistant",
          content:
            String.length(streamingText) > 0 ? streamingText : "Error: " ++ message,
        },
      |],
    )
  | _ => messages
  };

[@melange_only]
let messageListElement = () =>
  Webapi.Dom.document
  |> Webapi.Dom.Document.getElementById("message-list");

[@melange_only]
let scrollToBottomElement = el =>
  Webapi.Dom.Element.setScrollTop(
    el,
    float_of_int(Webapi.Dom.Element.scrollHeight(el)),
  );

[@melange_only]
let scrollToBottom = () => {
  switch (messageListElement()) {
  | Some(el) => scrollToBottomElement(el)
  | None => ()
  };
};

[@melange_only]
let isNearBottomElement = el => {
  let scrollTop = Webapi.Dom.Element.scrollTop(el);
  let scrollHeight = Webapi.Dom.Element.scrollHeight(el);
  let clientHeight = Webapi.Dom.Element.clientHeight(el);
  scrollTop +. float_of_int(clientHeight) > float_of_int(scrollHeight) -. 80.0;
};

[@melange_only]
let isNearBottom = () => {
  switch (messageListElement()) {
  | Some(el) => isNearBottomElement(el)
  | None => true
  };
};

[@native_only]
let messageListElement = () => None;

[@native_only]
let scrollToBottomElement = _el => ();

[@native_only]
let scrollToBottom = () => ();

[@native_only]
let isNearBottomElement = _el => true;

[@native_only]
let isNearBottom = () => true;

[@melange_only]
let cancelScheduledScroll = scrollRafId => {
  switch (scrollRafId^) {
  | Some(rafId) =>
    Webapi.cancelAnimationFrame(rafId);
    scrollRafId := None;
  | None => ()
  };
};

[@melange_only]
let scheduleScrollToBottom = (scrollRafId, shouldStickToBottomRef) => {
  let scrollIfSticky = () =>
    if (shouldStickToBottomRef^) {
      scrollToBottom();
    };
  cancelScheduledScroll(scrollRafId);
  scrollIfSticky();
  scrollRafId := Some(Webapi.requestCancellableAnimationFrame(_ => {
    scrollIfSticky();
    scrollRafId := Some(Webapi.requestCancellableAnimationFrame(_ => {
      scrollIfSticky();
      scrollRafId := None;
    }));
  }));
};

[@melange_only]
let useAutoScroll = (messages, isStreaming, streamingText) => {
  let shouldStickToBottomRef = React.useMemo1(() => ref(true), [||]);
  let scrollRafId = React.useMemo1(() => ref(None), [||]);

  React.useEffect0(() => {
    switch (messageListElement()) {
    | Some(el) =>
      shouldStickToBottomRef := isNearBottomElement(el);
      let onScroll = _event => {
        let shouldStick = isNearBottomElement(el);
        shouldStickToBottomRef := shouldStick;
        if (!shouldStick) {
          cancelScheduledScroll(scrollRafId);
        };
      };
      Webapi.Dom.Element.addEventListener("scroll", onScroll, el);
      /* Pin on DOM mutations while sticky: Streamdown reveals streamed content
         progressively, decoupled from store commits, so render-driven scrolls
         alone can lag behind growth by a full chunk. Scrolling in the same
         frame as the growth keeps the list pinned. */
      let observer =
        Webapi.Dom.MutationObserver.make((_records, _observer) =>
          if (shouldStickToBottomRef^) {
            scrollToBottom();
          }
        );
      Webapi.Dom.MutationObserver.observe(
        el,
        [%obj {childList: true, subtree: true, characterData: true}],
        observer,
      );
      Some(() => {
        Webapi.Dom.Element.removeEventListener("scroll", onScroll, el);
        Webapi.Dom.MutationObserver.disconnect(observer);
      });
    | None => None
    };
  });

  /* useLayoutEffect so the stick-scroll lands synchronously after the DOM
     commit (before paint); a passive effect can slip a frame behind content
     growth and briefly leave the list un-pinned during streaming. */
  React.useLayoutEffect3(
    () => {
      if (shouldStickToBottomRef^) {
        scheduleScrollToBottom(scrollRafId, shouldStickToBottomRef);
      };
      None;
    },
    (messages, isStreaming, streamingText),
  );
};

[@native_only]
let useAutoScroll = (_messages, _isStreaming, _streamingText) => ();

type connectionBanner = {
  disconnected: bool,
  pendingActions: int,
};

[@melange_only]
let useConnectionBanner = () => {
  let statusValue = LlmChatStore.Hooks.useConnectionStatus();
  let (hasConnectedOnce, setHasConnectedOnce) = React.useState(() => false);
  let (graceElapsed, setGraceElapsed) = React.useState(() => false);
  React.useEffect0(() => {
    switch (statusValue.connection) {
    | Open when !hasConnectedOnce => setHasConnectedOnce(_ => true)
    | _ => ()
    };
    None;
  });
  React.useEffect0(() => {
    let timerId =
      Js.Global.setTimeout(~f=() => setGraceElapsed(_ => true), 2500);
    Some(() => Js.Global.clearTimeout(timerId));
  });
  let disconnected =
    switch (statusValue.connection) {
    | Open => false
    /* Before the first successful open, only complain once the initial
       connection grace window has passed (avoids flashing on page load). */
    | WaitingForOpen => hasConnectedOnce || graceElapsed
    | NotApplicable => graceElapsed && !hasConnectedOnce
    };
  Some({disconnected, pendingActions: statusValue.pendingActions});
};

[@native_only]
let useConnectionBanner = () => None;

module ConnectionBanner = {
  [@react.component]
  let make = (~state: option(connectionBanner)) =>
    switch (state) {
    | Some({disconnected: true, pendingActions}) =>
      <div className="connection-banner" id="connection-banner">
        {React.string(
           pendingActions > 0
             ? "You're offline — "
               ++ string_of_int(pendingActions)
               ++ " message(s) queued. They'll send automatically when the connection returns."
             : "Connection lost — reconnecting…",
         )}
      </div>;
    | _ => React.null;
    };
};

module View = {
  [@react.component]
  let make =
    leaf(() => {
      useTilia();
      module Hooks = LlmChatStore.Hooks;
      open Hooks;
      let store = LlmChatStore.Context.useStore();
      let router = UniversalRouter.useRouter();
      let pathname = UniversalRouter.usePathname();
      let (draft, setDraft) = React.useState(() => "");
      let sendPromptMutation =
        useMutationResult((module LlmChatStore.Mutations.SendPrompt), ());
      let createThread =
        useMutation((module LlmChatStore.Mutations.CreateNewThread), ());
      let selectThread =
        useMutation((module LlmChatStore.Mutations.SelectThread), ());
      let deleteThread =
        useMutation((module LlmChatStore.Mutations.DeleteThread), ());

      let messages = store.state.messages;
      let threads = store.state.threads;
      let currentThreadId =
        switch (store.state.current_thread_id) {
        | Some(threadId) => threadId
        | None => ""
      };
      let streaming = useStreaming();
      let streamingText = streamingTextForThread(~currentThreadId, streaming);
      let currentStreamId =
        currentStreamIdForThread(~currentThreadId, streaming);
      let hasCurrentStreamRow =
        hasCurrentStreamAssistantMessage(~messages, streaming);
      let isStreaming = isStreamingForThread(~currentThreadId, streaming);
      let connectionBanner = useConnectionBanner();
      let renderedMessages =
        appendCurrentStreamMessage(
          ~messages,
          ~currentThreadId,
          ~currentStreamId,
          ~hasCurrentStreamRow,
          ~streamingText,
          ~streamError=streaming.streamError,
        );

      React.useEffect1(
        () => {
          setDraft(_ => "");
          let expectedPath =
            switch (store.state.current_thread_id) {
            | Some(id) => "/" ++ id
            | None => "/"
            };
          if (pathname != expectedPath) {
            router.push(expectedPath);
          };
          let rafId = Webapi.requestCancellableAnimationFrame(_ => {
            scrollToBottom();
          });
          Some(() => Webapi.cancelAnimationFrame(rafId));
        },
        [|currentThreadId|],
      );

      useAutoScroll(messages, isStreaming, streamingText);

      <>
        <ConnectionBanner state={connectionBanner} />
        <div className="chat-layout">
        <div className="thread-sidebar" id="thread-list">
          <button
            className="new-thread-button"
            id="new-thread-button"
            onClick={event => onNewChatClick(createThread, router, event)}>
            {React.string("New Chat")}
          </button>
          {threads
           ->Js.Array.map(~f=(thread: Model.Thread.t) => {
               let isActive =
                 switch (store.state.current_thread_id) {
                 | Some(currentId) => currentId == thread.id
                 | None => false
                 };
               <div
                 key={thread.id}
                 className={
                   "thread-item" ++ (isActive ? " thread-item--active" : "")
                 }
                 id={"thread-item-" ++ thread.id}
                 onClick={event =>
                   onThreadClick(router, selectThread, thread.id, event)
                 }>
                 <span className="thread-item-title">
                   {React.string(thread.title)}
                 </span>
                  <button
                    id={"delete-thread-" ++ thread.id}
                    className="thread-delete-button"
                    onClick={event =>
                      onDeleteThread(router, deleteThread, thread.id, event)
                    }>
                    <Lucide.IconTrash size=14 />
                  </button>
               </div>;
             })
           ->React.array}
        </div>
        <div className="chat-main">
          <div className="message-list" id="message-list">
            {
              if (
                Array.length(threads) == 0
                && String.length(currentThreadId) == 0
              ) {
                <div className="empty-state" id="no-threads-state">
                  {React.string(
                    "No conversations yet. Click 'New Chat' to start one.",
                  )}
                </div>;
              } else if (
                Array.length(messages) == 0
                && String.length(streamingText) == 0
              ) {
                <div className="empty-state" id="empty-thread-state">
                  {React.string("Send a message to start the conversation.")}
                </div>;
              } else {
                renderedMessages->Js.Array.map(~f=(message: Model.Message.t) => {
                  let roleClass =
                    message.role == "user"
                      ? "message--user" : "message--assistant";
                  let isLastMessage = {
                    let len = Array.length(renderedMessages);
                    len > 0 && renderedMessages[len - 1].id == message.id;
                  };
                  let children =
                    if (message.role == "assistant") {
                      let content =
                        displayAssistantContent(~message, ~streaming);
                      [|
                        Streamdown.make(
                          ~isAnimating=isStreaming && isLastMessage,
                          ~children=content,
                          (),
                        ),
                      |];
                    } else {
                      [|React.string(message.content)|];
                    };
                  <div
                    key={message.id}
                    className={"message " ++ roleClass}
                    role={message.role}>
                    {React.array(children)}
                  </div>;
                })
                ->React.array;
              }
            }
          </div>
          {String.length(currentThreadId) > 0
             ? <div className="chat-input-area">
                  <textarea
                    value=draft
                    placeholder="Type a message..."
                    id="prompt-input"
                    onChange={event => handleInputChange(setDraft, event)}
                    onKeyDown={
                      event =>
                        handleKeyDown(
                          store,
                          sendPromptMutation,
                          draft,
                          setDraft,
                          event,
                        )
                    }
                    rows=1
                  />
                  <button
                    id="send-button"
                    disabled={isStreaming || sendPromptMutation.loading}
                    onClick={_ =>
                      handleSend(
                        store,
                        sendPromptMutation,
                        draft,
                        setDraft,
                      )
                    }>
                    {React.string("Send")}
                  </button>
               </div>
             : React.null}
        </div>
      </div>
      </>;
    });
};

let make = (~params as _, ~searchParams as _, ()) => <View />;
