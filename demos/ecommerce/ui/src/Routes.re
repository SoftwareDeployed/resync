module StorefrontLayoutView = {
  [@react.component]
  let make = (~children) => {
    let main_store = Store.Context.useStore();

    switch (main_store.config.premise) {
    | Some(_) => children
    | None => <NotFound />
    };
  };
};

module StorefrontLayout = {
  let make = (~children, ~params as _, ~searchParams as _, ()) => <StorefrontLayoutView> children </StorefrontLayoutView>;
};

module LandingPage = {
  let make = (~params as _, ~searchParams as _, ()) => <Landing />;
};

module ItemPage = {
  let make = (~params as _, ~searchParams as _, ()) => <Landing />;
};

module CartLayout = {
  let make = (~children, ~params as _, ~searchParams as _, ()) => <Container> children </Container>;
};

module CartPage = {
  let make = (~params as _, ~searchParams, ()) => {
    <Card>
      <Cart showContents=true />
      <p className="m-2 text-sm text-slate-600">
        "Review your selected equipment before booking."->React.string
      </p>
      <UniversalRouter.RouteLink
        id="home"
        searchParams
        className="m-2 inline-block text-sm text-slate-700 underline underline-offset-4 hover:text-slate-900">
        "Continue browsing equipment"->React.string
      </UniversalRouter.RouteLink>
    </Card>;
  };
};

module NotFoundPage = {
  let make = (~pathname as _, ()) => <NotFound />;
};

let itemIdLabel = params =>
  switch (UniversalRouter.Params.find("id", params)) {
  | Some(id) => "Item " ++ id
  | None => "Item"
  };

let itemNameFromState = (~params, ~state: Store.t) =>
  switch (UniversalRouter.Params.find("id", params)) {
  | None => itemIdLabel(params)
  | Some(id) =>
    switch (
      Js.Array.find(
        ~f=(item: Model.InventoryItem.t) => item.id == id,
        state.config.inventory,
      )
    ) {
    | Some(item) => item.name
    | None => "Item " ++ id
    }
  };

let resolveItemTitle = (~pathname as _, ~params, ~searchParams as _) =>
  itemIdLabel(params) ++ " - Cloud Hardware Rental";

let resolveItemHeadTags = (~pathname as _, ~params, ~searchParams as _) => {
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

let resolveItemTitleWithState = (~pathname as _, ~params, ~searchParams as _, ~state: Store.t) =>
  itemNameFromState(~params, ~state) ++ " - Cloud Hardware Rental";

let resolveItemHeadTagsWithState = (~pathname as _, ~params, ~searchParams as _, ~state: Store.t) => {
  let itemLabel = itemNameFromState(~params, ~state);
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
            ~resolveTitleWithState=resolveItemTitleWithState,
            ~headTags=[
              UniversalRouter.metaTag(
                ~name="description",
                ~content="Inspect hardware details, availability, and pricing before booking.",
                (),
              ),
            ],
            ~resolveHeadTags=resolveItemHeadTags,
            ~resolveHeadTagsWithState=resolveItemHeadTagsWithState,
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

let homePath = (~basePath="/", ()) =>
  UniversalRouter.routeHref(~router, ~basePath, ~id="home", ())
  |> Option.value(~default=basePath);

let itemPath = (~basePath="/", ~id, ()) =>
  UniversalRouter.routeHref(
    ~router,
    ~basePath,
    ~id="item",
    ~params=UniversalRouter.Params.ofList([("id", id)]),
    (),
  )
  |> Option.value(~default=basePath);

let cartPath = (~basePath="/", ()) =>
  UniversalRouter.routeHref(~router, ~basePath, ~id="cart", ())
  |> Option.value(~default="/cart");
