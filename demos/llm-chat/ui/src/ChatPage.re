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
  router.push("/");
};

let onDeleteThread = (router: UniversalRouter.routerApi, threadId, event) => {
  React.Event.Mouse.stopPropagation(event);
  React.Event.Mouse.preventDefault(event);
  LlmChatStore.dispatch(DeleteThread(threadId));
  router.push("/");
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
let useAutoScroll = (messages, isStreaming) => {
  let isNearBottomRef = React.useMemo1(() => ref(true), [||]);
  let scrollRafId = React.useMemo1(() => ref(None), [||]);
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

  let deleteButton = (~dataTestId, ~onClick, ~children) => {
    let dict = Js.Dict.empty();
    Js.Dict.set(dict, "data-testid", jsString(dataTestId));
    Js.Dict.set(dict, "className", jsString("thread-delete-button"));
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

  let deleteButton = (~dataTestId, ~onClick, ~children) => {
    let props =
      ReactDOM.domProps(~onClick, ())
      @ [
        React.JSX.string("data-testid", "data-testid", dataTestId),
        React.JSX.string("className", "className", "thread-delete-button"),
      ];
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
                setStreamingText(_ => "");
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
                 <span className="thread-item-title">
                   {React.string(thread.title)}
                 </span>
                  {DOMHelpers.deleteButton(
                     ~dataTestId="delete-thread-" ++ thread.id,
                      ~onClick=
                        event => onDeleteThread(router, thread.id, event),
                     ~children=[|<Lucide.IconTrash size=14 />|],
                   )}
               </div>;
             })
           ->React.array}
        </div>
         <div className="chat-main">
           {DOMHelpers.messageList(
              Js.Array.concat(
                ~other=[|
                  if (String.length(streamingText) > 0) {
                    DOMHelpers.messageDiv(
                      ~key="streaming-message",
                      ~className="message message--assistant",
                      ~role="assistant",
                      ~dataTestId="streaming-message",
                      ~children=[|
                        Streamdown.make(
                          ~isAnimating=true,
                          ~children=streamingText,
                          (),
                        ),
                      |],
                    );
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
                  DOMHelpers.messageDiv(
                    ~key=message.id,
                    ~className="message " ++ roleClass,
                    ~role=message.role,
                    ~dataTestId="message-" ++ message.id,
                    ~children,
                  );
                }),
              ),
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
                      setIsStreaming,
                      event,
                    ),
              )}
             {DOMHelpers.sendButton(
                ~id="send-button",
                ~dataTestId="send-button",
                ~disabled=isStreaming,
                ~onClick=_ => handleSend(store, draft, setDraft, setIsStreaming),
                ~children=[|React.string("Send")|],
              )}
          </div>
        </div>
      </div>;
    });
};

let make = (~params as _, ~searchParams as _, ()) => <View />;
