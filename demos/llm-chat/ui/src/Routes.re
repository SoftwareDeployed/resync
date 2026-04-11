module NotFoundPage = {
  let make = (~pathname as _, ()) =>
    <div> {React.string("404 - Page not found")} </div>;
};

let router: UniversalRouter.t(LlmChatStore.t) =
  UniversalRouter.create(
    ~document=UniversalRouter.document(
      ~title="LLM Chat",
      ~stylesheets=[|"/style.css"|],
      ~scripts=[|"/app.js"|],
      (),
    ),
    ~notFound=(module NotFoundPage),
    [
      UniversalRouter.index(
        ~id="chat",
        ~page=(module ChatPage),
        (),
      ),
      UniversalRouter.route(
        ~id="thread",
        ~path=":threadId",
        ~page=(module ChatPage),
        [],
        (),
      ),
    ],
  );
