[@platform js]
module Style = {
  [%%raw "import \"./style.css\""];
  [%%raw "import \"streamdown/styles.css\""];
};

[@platform native]
module Style = {
  let _css = ();
};

open Tilia.React;

[@platform js]
let handleInputChange = (setDraft, event) => {
  let value = React.Event.Form.target(event)##value;
  setDraft(_ => value);
};

[@platform native]
let handleInputChange = (_setDraft, _event) => ();

[@platform js]
let handleSend = (store: LlmChatStore.t, prompt, setDraft, setIsStreaming) => {
  let threadId =
    switch (store.state.current_thread_id) {
    | Some(id) => id
    | None => ""
    };
  if (String.length(prompt) > 0 && String.length(threadId) > 0) {
    LlmChatStore.dispatch(
      SendPrompt({
        thread_id: threadId,
        prompt,
      }),
    );
    setDraft(_ => "");
    setIsStreaming(_ => true);
  };
};

[@platform native]
let handleSend = (_store, _prompt, _setDraft, _setIsStreaming) =>
  ();

let onThreadClick = (_router: UniversalRouter.routerApi, threadId, _event) => {
  LlmChatStore.dispatch(SelectThread(threadId));
  ReasonReactRouter.push("/" ++ threadId);
};

let onNewChatClick = (router: UniversalRouter.routerApi, _event) => {
  let uuid = UUID.make();
  LlmChatStore.dispatch(
    CreateNewThread({id: uuid, title: "New Chat"}),
  );
  router.push("/" ++ uuid);
};

let onDeleteThread = (_router: UniversalRouter.routerApi, threadId, event) => {
  React.Event.Mouse.stopPropagation(event);
  React.Event.Mouse.preventDefault(event);
  LlmChatStore.dispatch(DeleteThread(threadId));
};

[@platform js]
let handleKeyDown = (store, draft, setDraft, setIsStreaming, event) => {
  let key = React.Event.Keyboard.key(event);
  let shift = React.Event.Keyboard.shiftKey(event);
  if (key == "Enter" && !shift) {
    React.Event.Keyboard.preventDefault(event);
    handleSend(store, draft, setDraft, setIsStreaming);
  };
};

[@platform native]
let handleKeyDown = (_store, _draft, _setDraft, _setIsStreaming, _event) =>
  ();

let hasConfirmedAssistantMessage = (~messages, ~content) =>
  messages->Js.Array.some(~f=(message: Model.Message.t) =>
    message.role == "assistant" && message.content == content
  );

[@platform js]
let scrollToBottom = () => {
  switch (
    Webapi.Dom.document
    |> Webapi.Dom.Document.getElementById("message-list")
  ) {
  | Some(el) =>
    Webapi.Dom.Element.setScrollTop(
      el,
      float_of_int(Webapi.Dom.Element.scrollHeight(el)),
    );
  | None => ()
  };
};

[@platform js]
let isNearBottom = () => {
  switch (
    Webapi.Dom.document
    |> Webapi.Dom.Document.getElementById("message-list")
  ) {
  | Some(el) =>
    let scrollTop = Webapi.Dom.Element.scrollTop(el);
    let scrollHeight = Webapi.Dom.Element.scrollHeight(el);
    let clientHeight = Webapi.Dom.Element.clientHeight(el);
    scrollTop +. float_of_int(clientHeight) > float_of_int(scrollHeight) -. 80.0;
  | None => true
  };
};

[@platform js]
let useAutoScroll = (messages, isStreaming, streamingText) => {
  let isNearBottomRef = React.useMemo1(() => ref(true), [||]);
  let scrollRafId = React.useMemo1(() => ref(None), [||]);
  React.useEffect3(
    () => {
      isNearBottomRef := isNearBottom();
      if (isNearBottomRef^) {
        switch (scrollRafId^) {
        | Some(rafId) => Webapi.cancelAnimationFrame(rafId)
        | None => ()
        };
        scrollRafId := Some(Webapi.requestCancellableAnimationFrame(_ => {
          scrollToBottom();
        }));
      };
      None;
    },
    (messages, isStreaming, streamingText),
  );
};

[@platform native]
let useAutoScroll = (_messages, _isStreaming, _streamingText) => ();

module View = {
  [@react.component]
  let make =
    leaf(() => {
      useTilia();
      let store = LlmChatStore.Context.useStore();
      let router = UniversalRouter.useRouter();
      let pathname = UniversalRouter.usePathname();
      let (isStreaming, setIsStreaming) = React.useState(() => false);
      let (draft, setDraft) = React.useState(() => "");
      let (streamingText, setStreamingText) = React.useState(() => "");

      let messages = store.state.messages;
      let threads = store.state.threads;
      let currentThreadId =
        switch (store.state.current_thread_id) {
        | Some(threadId) => threadId
        | None => ""
        };

      React.useEffect0(() => {
        let listenerId =
          LlmChatStore.Events.listen(event => {
            switch (event) {
            | CustomEvent(json) =>
              let eventKind =
                StoreJson.optionalField(
                  ~json,
                  ~fieldName="event",
                  ~decode=Melange_json.Primitives.string_of_json,
                );
              switch (eventKind) {
              | Some("token_received") =>
                let token =
                  StoreJson.optionalField(
                    ~json,
                    ~fieldName="token",
                    ~decode=Melange_json.Primitives.string_of_json,
                  );
                switch (token) {
                | Some(t) => setStreamingText(prev => prev ++ t)
                | None => ()
                };
              | Some("stream_complete") =>
                setIsStreaming(_ => false);
              | Some("stream_error") =>
                setIsStreaming(_ => false);
                setStreamingText(_ => "");
              | _ => ()
              }
            | _ => ()
            }
          });
        Some(() => LlmChatStore.Events.unlisten(listenerId));
      });

      React.useEffect2(
        () => {
          if (
            String.length(streamingText) > 0
            && hasConfirmedAssistantMessage(~messages, ~content=streamingText)
          ) {
            setStreamingText(_ => "");
          };
          None;
        },
        (messages, streamingText),
      );

      React.useEffect1(
        () => {
          setDraft(_ => "");
          setStreamingText(_ => "");
          setIsStreaming(_ => false);
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

      <div className="chat-layout">
        <div className="thread-sidebar" id="thread-list">
          <button
            className="new-thread-button"
            id="new-thread-button"
            onClick={event => onNewChatClick(router, event)}>
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
                 onClick={event => onThreadClick(router, thread.id, event)}>
                 <span className="thread-item-title">
                   {React.string(thread.title)}
                 </span>
                  <button
                    id={"delete-thread-" ++ thread.id}
                    className="thread-delete-button"
                    onClick={event => onDeleteThread(router, thread.id, event)}>
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
                Js.Array.concat(
                  ~other=[|
                    if (
                      String.length(streamingText) > 0
                      && !hasConfirmedAssistantMessage(~messages, ~content=streamingText)
                    ) {
                      <div
                        key="streaming-message"
                        id="streaming-message"
                        className="message message--assistant"
                        role="assistant">
                        {
                          Streamdown.make(
                            ~isAnimating=true,
                            ~children=streamingText,
                            (),
                          )
                        }
                      </div>;
                    } else {
                      React.null
                    },
                  |],
                  messages->Js.Array.map(~f=(message: Model.Message.t) => {
                    let roleClass =
                      message.role == "user"
                        ? "message--user" : "message--assistant";
                    let isLastMessage = {
                      let len = Array.length(messages);
                      len > 0 && messages[len - 1].id == message.id;
                    };
                    let children =
                      if (message.role == "assistant") {
                        [|
                          Streamdown.make(
                            ~isAnimating=isStreaming && isLastMessage,
                            ~children=message.content,
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
                  }),
                )
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
                          draft,
                          setDraft,
                          setIsStreaming,
                          event,
                        )
                    }
                    rows=1
                  />
                  <button
                    id="send-button"
                    disabled=isStreaming
                    onClick={_ => handleSend(store, draft, setDraft, setIsStreaming)}>
                    {React.string("Send")}
                  </button>
               </div>
             : React.null}
        </div>
      </div>;
    });
};

let make = (~params as _, ~searchParams as _, ()) => <View />;
