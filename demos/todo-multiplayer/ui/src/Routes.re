module NotFoundPage = {
  let make = (~pathname as _, ()) =>
    <div> {React.string("404 - Page not found")} </div>;
};

module HomePageComponent = {
  let make = (~params as _, ~searchParams as _, ()) => <HomePage />;
};

let router: UniversalRouter.t(TodoStore.t) =
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
