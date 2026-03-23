module HomePageWrapper = {
  let make = (~params as _, ~searchParams as _, ()) => {
    <HomePage />;
  };
};

module NotFoundPage = {
  let make = (~pathname as _, ()) => {
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
