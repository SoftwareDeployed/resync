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
      let* threadRow =
        Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
          RealtimeSchema.Queries.GetThread.find_opt(
            (module Db),
            RealtimeSchema.Queries.GetThread.caqti_type,
            threadId,
          )
        );
      switch (threadRow) {
      | None => Lwt.return(UniversalRouterDream.NotFound)
      | Some(row) =>
        let thread = ({
          Model.Thread.id: row.id,
          title: row.title,
          updated_at: row.updated_at,
        }: Model.Thread.t);
        let* messageRows =
          Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
            RealtimeSchema.Queries.GetMessages.collect(
              (module Db),
              RealtimeSchema.Queries.GetMessages.caqti_type,
              threadId,
            )
          );
        let messages =
          Array.map(
            (msgRow: RealtimeSchema.Queries.GetMessages.row) => ({
              Model.Message.id: msgRow.id,
              thread_id: msgRow.thread_id,
              role: msgRow.role,
              content: msgRow.content,
            }: Model.Message.t),
            Array.of_list(messageRows),
          );
        let* threadRows =
          Dream.sql(request, (module Db: Caqti_lwt.CONNECTION) =>
            RealtimeSchema.Queries.GetThreads.collect(
              (module Db),
              RealtimeSchema.Queries.GetThreads.caqti_type,
              (),
            )
          );
        let threads =
          Array.map(
            (tr: RealtimeSchema.Queries.GetThreads.row) => ({
              Model.Thread.id: tr.id,
              title: tr.title,
              updated_at: tr.updated_at,
            }: Model.Thread.t),
            Array.of_list(threadRows),
          );
        let config: Model.t = {
          threads,
          current_thread_id: Some(threadId),
          messages,
          input: "",
          updated_at: thread.updated_at,
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
