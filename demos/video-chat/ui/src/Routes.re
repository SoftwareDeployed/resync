module NotFoundPage = {
  let make = (~pathname as _, ()) =>
    <div> {React.string("404 - Page not found")} </div>;
};

module HomePageComponent = {
  let make = (~params as _, ~searchParams as _, ()) => <HomePage />;
};

let router: UniversalRouter.t(VideoChatStore.t) =
  UniversalRouter.create(
    ~document=UniversalRouter.document(
      ~title="Video Chat",
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
        ~id="room",
        ~path="room/:roomId",
        ~page=(module RoomPage),
        [],
        (),
      ),
    ],
  );
