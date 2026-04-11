open Lwt.Syntax;

let isHexDigit = (char: char) =>
  switch (char) {
  | '0' .. '9'
  | 'a' .. 'f'
  | 'A' .. 'F' => true
  | _ => false
  };

let isUuid = (value: string) => {
  let length = String.length(value);
  if (Int.equal(length, 36)) {
    let rec loop = index =>
      if (Int.equal(index, length)) {
        true;
      } else {
        let char = String.get(value, index);
        if (
          Int.equal(index, 8)
          || Int.equal(index, 13)
          || Int.equal(index, 18)
          || Int.equal(index, 23)
        ) {
          Char.equal(char, '-') && loop(index + 1);
        } else {
          isHexDigit(char) && loop(index + 1);
        };
      };
    loop(0);
  } else {
    false;
  };
};

let getServerState = (context: UniversalRouterDream.serverContext(LlmChatStore.t)) => {
  let UniversalRouterDream.{basePath, request} = context;
  if (String.length(basePath) <= 1) {
    Lwt.return(UniversalRouterDream.NotFound);
  } else {
    let threadId = String.sub(basePath, 1, String.length(basePath) - 1);
    if (!isUuid(threadId)) {
      Lwt.return(UniversalRouterDream.NotFound);
    } else {
      let* threadInfo = Dream.sql(request, Database.Chat.get_thread(threadId));
      switch (threadInfo) {
      | None => Lwt.return(UniversalRouterDream.NotFound)
      | Some(_thread) =>
        let* messages = Dream.sql(request, Database.Chat.get_messages(threadId));
        let* threads = Dream.sql(request, Database.Chat.get_threads());
        let config: Model.t = {
          threads,
          current_thread_id: Some(threadId),
          messages,
          input: "",
          updated_at: _thread.updated_at,
        };
        let store = LlmChatStore.createStore(config);
        Lwt.return(UniversalRouterDream.State(store));
      };
    };
  };
};

let render = (~context, ~serverState: LlmChatStore.t, ()) => {
  let store = serverState;
  let serializedState = LlmChatStore.serializeState(serverState.state);
  let UniversalRouterDream.{
    basePath,
    pathname: serverPathname,
    search: serverSearch,
  } = context;
  let app =
    <UniversalRouter
      router=Routes.router
      state=store
      basePath
      serverPathname
      serverSearch
    />;
  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~basePath,
      ~pathname=serverPathname,
      ~search=serverSearch,
      ~serializedState,
      ~state=store,
      (),
    );
  <LlmChatStore.Context.Provider value=store>
    document
  </LlmChatStore.Context.Provider>;
};

let app =
  UniversalRouterDream.app(
    ~router=Routes.router,
    ~getServerState,
    ~render,
    (),
  );
