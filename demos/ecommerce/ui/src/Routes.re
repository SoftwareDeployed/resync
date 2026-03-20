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
  let make = (~children, ~params as _, ~query as _, ()) => <StorefrontLayoutView> children </StorefrontLayoutView>;
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

let itemNameFromState = (~params, ~state: Store.t) =>
  switch (UniversalRouter.Params.find("id", params)) {
  | None => itemIdLabel(params)
  | Some(id) =>
    switch (
      Js.Array.find(
        ~f=(item: Config.InventoryItem.t) => item.id == id,
        state.config.inventory,
      )
    ) {
    | Some(item) => item.name
    | None => "Item " ++ id
    }
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

let resolveItemTitleWithState = (~path as _, ~params, ~query as _, ~state: Store.t) =>
  itemNameFromState(~params, ~state) ++ " - Cloud Hardware Rental";

let resolveItemHeadTagsWithState = (~path as _, ~params, ~query as _, ~state: Store.t) => {
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
  UniversalRouter.href(~router, ~basePath, ~id="home", ())
  |> Option.value(~default=basePath);

let itemPath = (~basePath="/", ~id, ()) =>
  UniversalRouter.href(
    ~router,
    ~basePath,
    ~id="item",
    ~params=UniversalRouter.Params.ofList([("id", id)]),
    (),
  )
  |> Option.value(~default=basePath);

let cartPath = (~basePath="/", ()) =>
  UniversalRouter.href(~router, ~basePath, ~id="cart", ())
  |> Option.value(~default="/cart");
