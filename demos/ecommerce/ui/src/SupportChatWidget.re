open Tilia.React;
open Melange_json.Primitives;

let str = React.string;

/*
 * Live support widget: embeds a "chat with a support agent" experience in
 * the storefront. The agent is the llm-chat demo server (Ollama-backed)
 * running on its own origin; this widget connects a multiplexed websocket
 * to its /_events endpoint, creates a fresh support thread on open, and
 * renders the conversation including streamed assistant replies.
 *
 * Client-only: the server renders just the launcher button placeholder.
 */

module ChatMessage = {
  type t = {
    id: string,
    role: string,
    content: string,
  };
};

type status =
  | Connecting
  | Ready
  | Offline;

let statusLabel = st =>
  switch (st) {
  | Connecting => "Connecting..."
  | Ready => "Support agent ready"
  | Offline => "Reconnecting..."
  };

let messageOfJson = (json: StoreJson.json): option(ChatMessage.t) => {
  let id = StoreJson.optionalField(~json, ~fieldName="id", ~decode=string_of_json);
  let role = StoreJson.optionalField(~json, ~fieldName="role", ~decode=string_of_json);
  let content =
    StoreJson.optionalField(~json, ~fieldName="content", ~decode=string_of_json);
  switch (id, role, content) {
  | (Some(id), Some(role), Some(content)) => Some({id, role, content})
  | _ => None
  };
};

let upsertMessage = (messages: array(ChatMessage.t), message: ChatMessage.t) => {
  let exists =
    messages->Js.Array.some(~f=(m: ChatMessage.t) => m.id == message.id);
  if (exists) {
    messages->Js.Array.map(~f=(m: ChatMessage.t) => m.id == message.id ? message : m);
  } else {
    messages->Js.Array.concat(~other=[|message|]);
  };
};

let removeMessage = (messages: array(ChatMessage.t), id: string) =>
  messages->Js.Array.filter(~f=(m: ChatMessage.t) => m.id != id);

module Widget = {
  [@react.component]
  let make =
    leaf(() => {
      useTilia();
      let (open_, setOpen) = React.useState(() => false);
      let (messages, setMessages) = React.useState(() => [||]);
      let (draft, setDraft) = React.useState(() => "");
      let (streamingText, setStreamingText) = React.useState(() => "");
      let (streamingActive, setStreamingActive) = React.useState(() => false);
      let (status, setStatus) = React.useState(() => Connecting);
      let clientRef = React.useRef(None);
      let threadIdRef = React.useRef(None);

      let%melange_only sendAction = (~kind, ~payloadFields) => {
        switch (clientRef.current) {
        | Some(client) => {
            let actionId = UUID.make();
            let action =
              StoreJson.Object.make(dict => {
                StoreJson.Object.setString(dict, "kind", kind);
                StoreJson.Object.setJson(dict, "payload", StoreJson.Object.make(payloadFields));
              });
            let _ =
              RealtimeClientMultiplexed.Multiplexed.sendAction(
                ~actionId,
                ~action,
                client,
              );
            ();
          }
        | None => ()
        }
      };

      let%melange_only ensureThread = () => {
        let threadId = UUID.make();
        threadIdRef.current = Some(threadId);
        sendAction(
          ~kind="create_new_thread",
          ~payloadFields=dict => {
            StoreJson.Object.setString(dict, "id", threadId);
            StoreJson.Object.setString(dict, "title", "Storefront support");
          },
        );
        threadId;
      };

      let%melange_only connect = () => {
        let client =
          RealtimeClientMultiplexed.Multiplexed.make(
            ~eventUrl=Constants.event_url,
            ~baseUrl=Constants.llm_chat_base_url,
          );
        clientRef.current = Some(client);
        let threadId = ensureThread();

        let handle =
          RealtimeClientMultiplexed.Multiplexed.subscribe(
            ~channel="thread:" ++ threadId,
            ~onOpen=() => setStatus(_previous => Ready),
            ~onClose=() => setStatus(_previous => Offline),
            ~onSnapshot=(json: StoreJson.json) => {
              let snapshotMessages =
                switch (StoreJson.field(json, "messages")) {
                | Some(messagesJson) =>
                    switch (messagesJson->Melange_json.classify) {
                    | `List(items) =>
                        List.filter_map(
                        (item: Js.Json.t) => messageOfJson(item),
                        items,
                      )
                      ->Array.of_list
                    | _ => [||]
                    }
                | None => [||]
                };
              setMessages(_previous => snapshotMessages);
            },
            ~onPatch=(~payload: StoreJson.json, ~timestamp as _) => {
              let table =
                StoreJson.optionalField(
                  ~json=payload,
                  ~fieldName="table",
                  ~decode=string_of_json,
                );
              let action =
                StoreJson.optionalField(
                  ~json=payload,
                  ~fieldName="action",
                  ~decode=string_of_json,
                );
              let data =
                switch (StoreJson.field(payload, "data")) {
                | Some(data) => messageOfJson(data)
                | None => None
                };
              switch (table, action, data) {
              | (Some("messages"), Some("INSERT"), Some(message)) =>
                  setMessages(previous => upsertMessage(previous, message))
              | (Some("messages"), Some("UPDATE"), Some(message)) =>
                  setMessages(previous => upsertMessage(previous, message))
              | (Some("messages"), Some("DELETE"), _)
                when table == Some("messages") =>
                  let id =
                    StoreJson.optionalField(
                      ~json=payload,
                      ~fieldName="id",
                      ~decode=value => string_of_json(value),
                    );
                  switch (id) {
                  | Some(id) => setMessages(previous => removeMessage(previous, id))
                  | None => ()
                  };
              | _ => ()
              };
            },
            ~onAck=(_actionId, _result, _error) => (),
            ~onCustom=(json: StoreJson.json) => {
              let event =
                StoreJson.optionalField(
                  ~json,
                  ~fieldName="event",
                  ~decode=string_of_json,
                );
              switch (event) {
              | Some("stream_started") =>
                  setStreamingActive(_previous => true);
                  setStreamingText(_previous => "");
              | Some("token_received") =>
                  let token =
                    StoreJson.optionalField(
                      ~json,
                      ~fieldName="token",
                      ~decode=string_of_json,
                    );
                  switch (token) {
                  | Some(token) =>
                      setStreamingText(previous => previous ++ token)
                  | None => ()
                  };
              | Some("stream_complete") =>
                  setStreamingActive(_previous => false);
                  setStreamingText(_previous => "");
              | Some("stream_error") =>
                  let error =
                    StoreJson.optionalField(
                      ~json,
                      ~fieldName="error",
                      ~decode=string_of_json,
                    );
                  setStreamingActive(_previous => false);
                  setStreamingText(_previous =>
                    switch (error) {
                    | Some(error) => "(error: " ++ error ++ ")"
                    | None => "(stream error)"
                    }
                  );
              | _ => ()
              };
            },
            ~onError=_message => setStatus(_previous => Offline),
            client,
          );
        ignore(handle);
        ();
      };

      let%melange_only toggleOpen = _event => {
        let next = !open_;
        setOpen(_previous => next);
        if (next && clientRef.current == None) {
          connect();
        };
      };

      let%melange_only sendPrompt = _event => {
        let prompt = draft->String.trim;
        if (prompt != "") {
          setDraft(_previous => "");
          switch (threadIdRef.current) {
          | Some(threadId) => {
              let messageId = UUID.make();
              let assistantMessageId = UUID.make();
              setMessages(previous =>
                upsertMessage(
                  previous,
                  {id: messageId, role: "user", content: prompt},
                )
              );
              setMessages(previous =>
                upsertMessage(
                  previous,
                  {
                    id: assistantMessageId,
                    role: "assistant",
                    content: "",
                  },
                )
              );
              sendAction(
                ~kind="send_prompt",
                ~payloadFields=dict => {
                  StoreJson.Object.setString(dict, "message_id", messageId);
                  StoreJson.Object.setString(
                    dict,
                    "assistant_message_id",
                    assistantMessageId,
                  );
                  StoreJson.Object.setString(dict, "thread_id", threadId);
                  StoreJson.Object.setString(dict, "prompt", prompt);
                },
              );
            }
          | None => ()
          };
        };
      };

      let%melange_only onDraftChange = event => {
        let value = React.Event.Form.target(event)##value;
        setDraft(_previous => value);
      };

      <div className="fixed bottom-4 right-4 z-50 flex flex-col items-end gap-2">
        {open_
           ? <div
               id="support-chat-panel"
               className="flex h-[28rem] w-80 flex-col overflow-hidden rounded-md border border-slate-300 bg-white shadow-xl">
               <div className="flex items-center justify-between bg-slate-700 px-3 py-2 text-white">
                 <span className="text-sm font-semibold">
                  <Lucide.IconMessageCircle size={16} className="mr-1 inline" />
                   {"Live support"->str}
                 </span>
                 <span
                   id="support-chat-status"
                   className={
                     "text-xs " ++ (status == Ready ? "text-green-300" : "text-amber-300")
                   }>
                   {statusLabel(status)->str}
                 </span>
               </div>
               <div
                 id="support-chat-messages"
                 className="flex flex-1 flex-col gap-2 overflow-y-auto p-3">
                 {
                   switch (messages->Array.length) {
                   | 0 =>
                       <p id="support-chat-empty" className="text-xs text-slate-500">
                         {"Ask about availability, pricing, or your reservation."->str}
                       </p>
                   | _ =>
                       messages
                       ->Js.Array.map(~f=(message: ChatMessage.t) => {
                            let isUser = message.role == "user";
                            let content =
                              if (!isUser && message.content == "" && streamingActive) {
                                streamingText == ""
                                  ? "..." : streamingText;
                              } else if (
                                !isUser && streamingActive && message.content != ""
                              ) {
                                streamingText != ""
                                  && String.length(streamingText)
                                    > String.length(message.content)
                                  ? streamingText : message.content;
                              } else {
                                message.content;
                              };
                            <div
                              key=message.id
                              className={
                                "max-w-[85%] rounded-md px-2 py-1 text-sm "
                                ++ (
                                  isUser
                                    ? "self-end bg-slate-600 text-white"
                                    : "self-start bg-slate-100 text-slate-800"
                                )
                              }>
                              {content->str}
                            </div>;
                          })
                       |> React.array
                   }
                 }
               </div>
               <div className="flex items-center gap-2 border-t border-slate-200 p-2">
                 <input
                   id="support-chat-input"
                   type_="text"
                   placeholder="Type a message..."
                   value=draft
                   onChange=onDraftChange
                   className="flex-1 rounded-sm border border-slate-300 px-2 py-1 text-sm focus:border-slate-500 focus:outline-none"
                 />
                 <button
                   id="support-chat-send"
                   type_="button"
                   className="rounded-sm bg-slate-600 px-3 py-1 text-sm text-white hover:bg-slate-700"
                   onClick=sendPrompt>
                   {"Send"->str}
                 </button>
               </div>
             </div>
           : React.null}
        <button
          id="support-chat-toggle"
          type_="button"
          className="flex items-center gap-2 rounded-full bg-slate-700 px-4 py-3 text-sm font-medium text-white shadow-lg transition-colors hover:bg-slate-800"
          onClick=toggleOpen>
          <Lucide.IconMessageCircle size={18} />
          {"Support"->str}
        </button>
      </div>;
    });
};

[@react.component]
let make = () =>
  <ClientOnly>
    {() => <Widget />}
  </ClientOnly>;
