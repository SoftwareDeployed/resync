module HomePageWrapper = {
  let make = (~params as _, ~searchParams as _, ()) => <HomePage />;
};

module NotFoundPage = {
  let make = (~pathname as _, ()) => {
    <main className="poker-app">
      <div className="rail-card">
        <h1> {React.string("Table not found")} </h1>
        <p> {React.string("Return to the River Room at /.")} </p>
      </div>
    </main>;
  };
};

let router: UniversalRouter.t(unit) =
  UniversalRouter.create(
    ~document=
      UniversalRouter.document(
        ~title="River Room - Resync Poker",
        ~stylesheets=[|"/style.css"|],
        ~scripts=[|"/app.js"|],
        (),
      ),
    ~notFound=(module NotFoundPage),
    [UniversalRouter.index(~id="home", ~page=(module HomePageWrapper), ())],
  );
