module StorefrontLayout = {
  let make = (~children, ~params as _, ~query as _, ()) => {
    let main_store = StoreContext.useStore();

    switch (main_store.config.premise) {
    | Some(_) => children
    | None => <NotFound />
    };
  };
};

module LandingPage = {
  let make = (~params as _, ~query as _, ()) => <Landing />;
};

module ItemPage = {
  let make = (~params as _, ~query as _, ()) => <Landing />;
};

module CartLayout = {
  let make = (~children, ~params as _, ~query as _, ()) => <Container> children </Container>;
};

module CartPage = {
  let make = (~params as _, ~query as _, ()) => {
    <Card>
      <Cart showContents=true />
      <p className="m-2 text-sm text-slate-600">
        "Review your selected equipment before booking."->React.string
      </p>
      <UniversalRouter.Link
        id="home"
        className="m-2 inline-block text-sm text-slate-700 underline underline-offset-4 hover:text-slate-900">
        "Continue browsing equipment"->React.string
      </UniversalRouter.Link>
    </Card>;
  };
};

module NotFoundPage = {
  let make = (~path as _, ()) => <NotFound />;
};

let itemIdLabel = params =>
  switch (UniversalRouter.Params.find("id", params)) {
  | Some(id) => "Item " ++ id
  | None => "Item"
  };

let resolveItemTitle = (~path as _, ~params, ~query as _) =>
  itemIdLabel(params) ++ " - Cloud Hardware Rental";

let resolveItemHeadTags = (~path as _, ~params, ~query as _) => {
  let itemLabel = itemIdLabel(params);
  [
    UniversalRouter.propertyTag(
      ~property="og:title",
      ~content=itemLabel ++ " - Cloud Hardware Rental",
      (),
    ),
    UniversalRouter.metaTag(
      ~name="description",
      ~content=itemLabel ++ " includes pricing and availability details before booking.",
      (),
    ),
  ];
};

let router =
  UniversalRouter.create(
    ~document=
      UniversalRouter.document(
        ~title="Cloud Hardware Rental",
        ~stylesheets=[|"/style.css"|],
        ~scripts=[|"/app.js"|],
        ~headTags=[
          UniversalRouter.metaTag(
            ~name="description",
            ~content="Reserve cloud hardware with flexible dates and live availability.",
            (),
          ),
          UniversalRouter.metaTag(~name="robots", ~content="index,follow", ()),
        ],
        (),
      ),
    ~notFound=(module NotFoundPage),
    [
      UniversalRouter.group(
        ~path="",
        ~layout=(module StorefrontLayout),
        [
          UniversalRouter.index(
            ~id="home",
            ~title="Cloud Hardware Rental",
            ~headTags=[
              UniversalRouter.metaTag(
                ~name="description",
                ~content="Browse rentable cloud hardware and compare reservation pricing.",
                (),
              ),
              UniversalRouter.propertyTag(
                ~property="og:title",
                ~content="Cloud Hardware Rental",
                (),
              ),
            ],
            ~page=(module LandingPage),
            (),
          ),
          UniversalRouter.route(
            ~id="item",
            ~path="item/:id",
            ~title="Item",
            ~resolveTitle=resolveItemTitle,
            ~headTags=[
              UniversalRouter.metaTag(
                ~name="description",
                ~content="Inspect hardware details, availability, and pricing before booking.",
                (),
              ),
            ],
            ~resolveHeadTags=resolveItemHeadTags,
            ~page=(module ItemPage),
            [],
            (),
          ),
        ],
        (),
      ),
      UniversalRouter.group(
        ~path="cart",
        ~layout=(module CartLayout),
        [
          UniversalRouter.index(
            ~id="cart",
            ~title="Cart",
            ~headTags=[
              UniversalRouter.metaTag(
                ~name="description",
                ~content="Review your selected equipment before booking your reservation.",
                (),
              ),
            ],
            ~page=(module CartPage),
            (),
          ),
        ],
        (),
      ),
    ],
  );

let homePath = (~routeRoot="/", ()) =>
  UniversalRouter.href(~router, ~routeRoot, ~id="home", ())
  |> Option.value(~default=routeRoot);

let itemPath = (~routeRoot="/", ~id, ()) =>
  UniversalRouter.href(
    ~router,
    ~routeRoot,
    ~id="item",
    ~params=UniversalRouter.Params.ofList([("id", id)]),
    (),
  )
  |> Option.value(~default=routeRoot);

let cartPath = (~routeRoot="/", ()) =>
  UniversalRouter.href(~router, ~routeRoot, ~id="cart", ())
  |> Option.value(~default="/cart");
