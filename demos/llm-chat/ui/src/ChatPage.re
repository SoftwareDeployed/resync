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
let handleSend =
    (store: LlmChatStore.t, prompt, setDraft, sseBufferRef, setIsStreaming) => {
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

    let messagesJson =
      store.state.messages
      ->Js.Array.map(~f=(msg: Model.Message.t) =>
          "{\"role\":"
          ++ Melange_json.to_string(Melange_json.To_json.string(msg.role))
          ++ ",\"content\":"
          ++ Melange_json.to_string(
               Melange_json.To_json.string(msg.content),
             )
          ++ "}"
        )
      ->Js.Array.concat(
          ~other=[|
            "{\"role\":\"user\",\"content\":"
            ++ Melange_json.to_string(Melange_json.To_json.string(prompt))
            ++ "}",
          |],
        )
      ->Js.Array.reduce(~f=(a, b) => a == "" ? b : a ++ "," ++ b, ~init="");

    let body =
      "{\"thread_id\":"
      ++ Melange_json.to_string(Melange_json.To_json.string(threadId))
      ++ ",\"messages\":["
      ++ messagesJson
      ++ "]}";

    let _unsubscribe =
      StreamPipe.subscribe(
        StreamPipeFetch.post(~url=Constants.base_url ++ "/api/chat", ~body),
        event => {
        switch (event) {
        | Error(msg) =>
          setIsStreaming(_ => false);
          LlmChatStore.dispatch(SetError(msg));
        | Done =>
          setIsStreaming(_ => false);
          LlmChatStore.dispatch(FinishResponse);
        | Chunk(chunk) =>
          let events = SseParser.parseChunk(chunk, ~buffer=sseBufferRef);
          events->Js.Array.forEach(~f=event => {
            switch (StoreJson.tryParse(event.data)) {
            | Some(json) =>
              switch (StoreJson.field(json, "type")) {
              | Some(typeJson) =>
                switch (Melange_json.Primitives.string_of_json(typeJson)) {
                | "token" =>
                  switch (StoreJson.field(json, "content")) {
                  | Some(contentJson) =>
                    let content =
                      Melange_json.Primitives.string_of_json(contentJson);
                    LlmChatStore.dispatch(AppendToken(content));
                  | None => ()
                  }
                | "done" =>
                  setIsStreaming(_ => false);
                  LlmChatStore.dispatch(FinishResponse);
                | _ => ()
                }
              | None => ()
              }
            | None => ()
            }
          });
        }
      });
    ();
  };
};

[@platform native]
let handleSend = (_store, _prompt, _setDraft, _sseBufferRef, _setIsStreaming) =>
  ();

let onThreadClick = (router: UniversalRouter.routerApi, threadId, _event) => {
  LlmChatStore.dispatch(SelectThread(threadId));
  router.push("/" ++ threadId);
};

let onNewChatClick = (router: UniversalRouter.routerApi, _event) => {
  router.push("/");
};

[@platform js]
let handleKeyDown =
    (store, draft, setDraft, sseBufferRef, setIsStreaming, event) => {
  let key = React.Event.Keyboard.key(event);
  let shift = React.Event.Keyboard.shiftKey(event);
  if (key == "Enter" && !shift) {
    React.Event.Keyboard.preventDefault(event);
    handleSend(store, draft, setDraft, sseBufferRef, setIsStreaming);
  };
};

[@platform native]
let handleKeyDown =
    (_store, _draft, _setDraft, _sseBufferRef, _setIsStreaming, _event) =>
  ();

[@platform js]
let useSseBufferRef = () => React.useMemo1(() => ref(""), [||]);

[@platform native]
let useSseBufferRef = () => ref("");

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
let scrollRafId: ref(option(Webapi.rafId)) = ref(None);

[@platform js]
let useAutoScroll = (messages, isStreaming) => {
  let isNearBottomRef: ref(bool) = ref(true);
  React.useEffect2(
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
    (messages, isStreaming),
  );
};

[@platform native]
let useAutoScroll = (_messages, _isStreaming) => ();

[@platform js]
module DOMHelpers = {
  type jsValue;
  external jsString: string => jsValue = "%identity";
  external jsBool: bool => jsValue = "%identity";
  external jsFn: ('a => 'b) => jsValue = "%identity";
  external unsafeDomProps: Js.Dict.t(jsValue) => ReactDOM.domProps =
    "%identity";

  let messageList = children => {
    let dict = Js.Dict.empty();
    Js.Dict.set(dict, "className", jsString("message-list"));
    Js.Dict.set(dict, "id", jsString("message-list"));
    Js.Dict.set(dict, "data-testid", jsString("message-list"));
    ReactDOM.createElement("div", ~props=unsafeDomProps(dict), children);
  };

  let messageDiv = (~key, ~className, ~role, ~dataTestId, ~children) => {
    let dict = Js.Dict.empty();
    Js.Dict.set(dict, "key", jsString(key));
    Js.Dict.set(dict, "className", jsString(className));
    Js.Dict.set(dict, "role", jsString(role));
    Js.Dict.set(dict, "data-testid", jsString(dataTestId));
    ReactDOM.createElement("div", ~props=unsafeDomProps(dict), children);
  };

  let promptInput =
      (~value, ~placeholder, ~id, ~dataTestId, ~onChange, ~onKeyDown) => {
    let dict = Js.Dict.empty();
    Js.Dict.set(dict, "value", jsString(value));
    Js.Dict.set(dict, "placeholder", jsString(placeholder));
    Js.Dict.set(dict, "id", jsString(id));
    Js.Dict.set(dict, "data-testid", jsString(dataTestId));
    Js.Dict.set(dict, "onChange", jsFn(onChange));
    Js.Dict.set(dict, "onKeyDown", jsFn(onKeyDown));
    Js.Dict.set(dict, "rows", jsString("1"));
    ReactDOM.createElement("textarea", ~props=unsafeDomProps(dict), [||]);
  };

  let sendButton = (~id, ~dataTestId, ~disabled, ~onClick, ~children) => {
    let dict = Js.Dict.empty();
    Js.Dict.set(dict, "id", jsString(id));
    Js.Dict.set(dict, "data-testid", jsString(dataTestId));
    Js.Dict.set(dict, "disabled", jsBool(disabled));
    Js.Dict.set(dict, "onClick", jsFn(onClick));
    ReactDOM.createElement("button", ~props=unsafeDomProps(dict), children);
  };
};

[@platform native]
module DOMHelpers = {
  let messageList = children =>
    ReactDOM.createDOMElementVariadic(
      "div",
      ~props=
        ReactDOM.domProps(~className="message-list", ~id="message-list", ()),
      children,
    );

  let messageDiv = (~key, ~className, ~role, ~dataTestId, ~children) => {
    let props =
      ReactDOM.domProps(~className, ~role, ())
      @ [React.JSX.string("data-testid", "data-testid", dataTestId)];
    React.createElementWithKey(~key, "div", props, Array.to_list(children));
  };

  let promptInput =
      (~value, ~placeholder, ~id, ~dataTestId, ~onChange, ~onKeyDown) => {
    let props =
      ReactDOM.domProps(~value, ~placeholder, ~id, ~onChange, ~onKeyDown, ())
      @ [React.JSX.string("data-testid", "data-testid", dataTestId)];
    ReactDOM.createDOMElementVariadic("textarea", ~props, [||]);
  };

  let sendButton = (~id, ~dataTestId, ~disabled, ~onClick, ~children) => {
    let props =
      ReactDOM.domProps(~id, ~disabled, ~onClick, ())
      @ [React.JSX.string("data-testid", "data-testid", dataTestId)];
    ReactDOM.createDOMElementVariadic("button", ~props, children);
  };
};

module View = {
  [@react.component]
  let make =
    leaf(() => {
      useTilia();
      let store = LlmChatStore.Context.useStore();
      let router = UniversalRouter.useRouter();
      let sseBufferRef = useSseBufferRef();
      let (isStreaming, setIsStreaming) = React.useState(() => false);
      let (draft, setDraft) = React.useState(() => "");

      let messages = store.state.messages;
      let threads = store.state.threads;
      let currentThreadId =
        switch (store.state.current_thread_id) {
        | Some(threadId) => threadId
        | None => ""
        };

      React.useEffect1(
        () => {
          setDraft(_ => "");
          let rafId = Webapi.requestCancellableAnimationFrame(_ => {
            scrollToBottom();
          });
          Some(() => Webapi.cancelAnimationFrame(rafId));
        },
        [|currentThreadId|],
      );

      useAutoScroll(messages, isStreaming);

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
                 {React.string(thread.title)}
               </div>;
             })
           ->React.array}
        </div>
        <div className="chat-main">
          {DOMHelpers.messageList(
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
               DOMHelpers.messageDiv(
                 ~key=message.id,
                 ~className="message " ++ roleClass,
                 ~role=message.role,
                 ~dataTestId="message-" ++ message.id,
                 ~children,
               );
             }),
           )}
          <div className="chat-input-area">
            {DOMHelpers.promptInput(
               ~value=draft,
               ~placeholder="Type a message...",
               ~id="prompt-input",
               ~dataTestId="prompt-input",
               ~onChange=event => handleInputChange(setDraft, event),
               ~onKeyDown=
                 event =>
                   handleKeyDown(
                     store,
                     draft,
                     setDraft,
                     sseBufferRef,
                     setIsStreaming,
                     event,
                   ),
             )}
            {DOMHelpers.sendButton(
               ~id="send-button",
               ~dataTestId="send-button",
               ~disabled=isStreaming,
               ~onClick=
                 _ =>
                   handleSend(
                     store,
                     draft,
                     setDraft,
                     sseBufferRef,
                     setIsStreaming,
                   ),
               ~children=[|React.string("Send")|],
             )}
          </div>
        </div>
      </div>;
    });
};

let make = (~params as _, ~searchParams as _, ()) => <View />;
