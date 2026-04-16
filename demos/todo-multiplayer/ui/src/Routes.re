module NotFoundPage = {
  let make = (~pathname as _, ()) =>
    <div> {React.string("404 - Page not found")} </div>;
};

module HomePageComponent = {
  let make = (~params, ~searchParams as _, ()) => {
    let listId =
      switch (UniversalRouter.Params.find("listId", params)) {
      | Some(id) => id
      | None => ""
      };
    <HomePage listId />;
  };
};

// Server state type from EntryServer
type serverState = {
  store: TodoStore.t,
  serializedQueries: string,
};

let router: UniversalRouter.t(serverState) =
  UniversalRouter.create(
    ~document = UniversalRouter.document(
      ~title="Todo Multiplayer",
      ~stylesheets=[|"/style.css"|],
      ~scripts=[|"/app.js"|],
      (),
    ),
    ~notFound=(module NotFoundPage),
    [
      UniversalRouter.index(
        ~id="home",
        ~page=(module HomePageComponent),
        (),
      ),
      UniversalRouter.route(
        ~id="list",
        ~path=":listId",
        ~page=(module HomePageComponent),
        [],
        (),
      ),
    ],
  );
