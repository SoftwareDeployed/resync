module HomePageWrapper = {
  let make = (~params as _, ~query as _, ()) => {
    <HomePage />;
  };
};

module NotFoundPage = {
  let make = (~path as _, ()) => {
    <div> {React.string("404 - Page not found")} </div>;
  };
};

let router: UniversalRouter.t(TodoStore.t) =
  UniversalRouter.create(
    ~document=
      UniversalRouter.document(
        ~title="Todo App",
        ~stylesheets=[|"/style.css"|],
        ~scripts=[|"/app.js"|],
        (),
      ),
    ~notFound=(module NotFoundPage),
    [UniversalRouter.index(~id="home", ~page=(module HomePageWrapper), ())],
  );
