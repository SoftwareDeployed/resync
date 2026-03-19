# universal-reason-react/router

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.


Shared nested routing for Dream and Reason React applications.

## Overview

The Universal Router allows you to define your routes once and use them in both server-side rendering (Dream) and client-side navigation. This eliminates route duplication and ensures consistency between SSR and client routing.

## Features

- **Single Route Definition**: Define routes once, use everywhere
- **Nested Layouts**: Support for layout composition and nesting
- **Type-Safe Routing**: Compile-time route matching and parameter validation
- **SSR Integration**: Seamless server-side rendering with Dream
- **Client Navigation**: Smooth client-side transitions without page reloads
- **State-Aware Metadata**: Resolve document title and meta tags from app state

## Quick Start

### Basic Route Setup

```reason
// Routes.re
open UniversalRouter;

let router =
  create(
    ~document=document(~title="My App", ()),
    ~notFound=(module NotFoundPage),
    [
      index(~id="home", ~page=(module HomePage), ()),
      route(
        ~id="about",
        ~path="about",
        ~page=(module AboutPage),
        [],
        (),
      ),
    ],
  );
```

### Nested Layouts

```reason
let router =
  UniversalRouter.create(
    ~document=UniversalRouter.document(~title="Dashboard", ()),
    ~notFound=(module NotFoundPage),
    [
      UniversalRouter.group(
        ~path="",
        ~layout=(module RootLayout),
        [
          // Public routes
          UniversalRouter.index(~id="home", ~page=(module HomePage), ()),
          
          // Protected routes with auth layout
          UniversalRouter.group(
            ~path="dashboard",
            ~layout=(module AuthLayout),
            [
              UniversalRouter.index(~id="dashboard", ~page=(module DashboardPage), ()),
              UniversalRouter.route(
                ~id="profile",
                ~path="profile",
                ~page=(module ProfilePage),
                [],
                (),
              ),
              UniversalRouter.route(
                ~id="settings",
                ~path="settings",
                ~page=(module SettingsPage),
                [],
                (),
              ),
            ],
            (),
          ),
        ],
        (),
      ),
    ],
  );
```

### Route Parameters

```reason
let router =
  UniversalRouter.create(
    ~document=UniversalRouter.document(~title="Store", ()),
    ~notFound=(module NotFoundPage),
    [
      // Single parameter
      UniversalRouter.route(
        ~id="product",
        ~path="product/:id",
        ~page=(module ProductPage),
        [],
        (),
      ),
      
      // Multiple parameters
      UniversalRouter.route(
        ~id="category",
        ~path="category/:categoryId/product/:productId",
        ~page=(module CategoryProductPage),
        [],
        (),
      ),
    ],
  );
```

### State-Aware Document Metadata

Resolve page titles and meta tags dynamically from your application state:

```reason
// Routes.re
open UniversalRouter;

// Static title (simple string)
let resolveItemTitle = (~path as _, ~params, ~query as _) => {
  let itemId = params |> Params.find("id") |> Belt.Option.getWithDefault("Unknown");
  "Item " ++ itemId ++ " - My Store";
};

// Dynamic title from app state
let resolveItemTitleWithState = (~path as _, ~params, ~query as _, ~state: Store.t) => {
  let itemId = params |> Params.find("id") |> Belt.Option.getWithDefault("");
  
  // Look up item name from the store
  switch (Js.Array.find(~f=(item: Item.t) => item.id == itemId, state.config.inventory)) {
  | Some(item) => item.name ++ " - My Store"
  | None => "Item Not Found - My Store"
  };
};

// Static head tags
let resolveItemHeadTags = (~path as _, ~params, ~query as _) => {
  let itemId = params |> Params.find("id") |> Belt.Option.getWithDefault("");
  [
    metaTag(~name="description", ~content="View item details", ()),
    propertyTag(~property="og:title", ~content="Item Details", ()),
  ];
};

// Dynamic head tags from app state
let resolveItemHeadTagsWithState = (~path as _, ~params, ~query as _, ~state: Store.t) => {
  let itemId = params |> Params.find("id") |> Belt.Option.getWithDefault("");
  
  switch (Js.Array.find(~f=(item: Item.t) => item.id == itemId, state.config.inventory)) {
  | Some(item) => [
      metaTag(~name="description", ~content=item.description, ()),
      propertyTag(~property="og:title", ~content=item.name, ()),
      propertyTag(~property="og:image", ~content=item.imageUrl, ()),
    ]
  | None => [
      metaTag(~name="description", ~content="Item not found", ()),
    ]
  };
};

let router =
  UniversalRouter.create(
    ~document=UniversalRouter.document(~title="My Store", ()),
    ~notFound=(module NotFoundPage),
    [
      // Route with static metadata
      UniversalRouter.route(
        ~id="item",
        ~path="item/:id",
        ~page=(module ItemPage),
        ~resolveTitle=resolveItemTitle,
        ~resolveHeadTags=resolveItemHeadTags,
        [],
        (),
      ),
      
      // Route with state-aware metadata
      UniversalRouter.route(
        ~id="item",
        ~path="item/:id",
        ~page=(module ItemPage),
        ~resolveTitleWithState=resolveItemTitleWithState,
        ~resolveHeadTagsWithState=resolveItemHeadTagsWithState,
        [],
        (),
      ),
    ],
  );
```

**Note:** When using state-aware metadata resolution, you must pass the `state` prop to the `UniversalRouter` component:

```reason
// Server-side rendering
let render = (~context, ~serverState: Store.t, ()) => {
  let app =
    <UniversalRouter
      router=Routes.router
      state=serverState  // Required for state-aware metadata
      routeRoot
      serverPath
      serverSearch
    />;
  
  // ...
};

// Client-side hydration
let store = Store.hydrateStore();

ReactDOM.hydrateRoot(
  root,
  <UniversalRouter
    router=Routes.router
    state=store  // Required for state-aware metadata
  />,
);
```

### Server-Side Integration

### Dream Handler

```reason
// server.ml
let () =
  Dream.run
  @@ Dream.logger
  @@ Dream.router([
    Dream.get "/**" (
      UniversalRouterDream.handler(~app=EntryServer.app)
    ),
  ]);
```

### Server Entry Module

```reason
// EntryServer.re

// Example: Route-based data fetching
let fetchDataForRoute = (routeRoot: string, request: Dream.request) => {
  switch (routeRoot) {
  | "/" =>
    // Home page - fetch featured items
    let* featuredItems = Dream.sql(request, Database.getFeaturedItems());
    Lwt.return({items: featuredItems, user: None})
    
  | "/products" =>
    // Products page - fetch all products
    let* products = Dream.sql(request, Database.getAllProducts());
    Lwt.return({items: products, user: None})
    
  | "/products/:id" =>
    // Product detail - fetch single product
    let productId = extractParam(routeRoot, "id");
    let* product = Dream.sql(request, Database.getProductById(productId));
    Lwt.return({items: Option.to_list(product), user: None})
    
  | "/dashboard" =>
    // Dashboard - fetch user-specific data
    let* user = authenticate(request);
    switch (user) {
    | Some(userId) =>
      let* userData = Dream.sql(request, Database.getUserData(userId));
      let* orders = Dream.sql(request, Database.getUserOrders(userId));
      Lwt.return({user: Some(userData), orders: orders})
    | None =>
      Lwt.return({user: None, orders: []})
    }
    
  | _ =>
    // Default - return empty state
    Lwt.return({items: [], user: None})
  };
};

let getServerState = (context: UniversalRouterDream.serverContext(Store.t)) => {
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let request = UniversalRouterDream.contextRequest(context);
  
  // Fetch data based on the current route
  let* data = fetchDataForRoute(routeRoot, request);
  
  // Create store from data
  let store = Store.createStore(data);
  Lwt.return(UniversalRouterDream.State(store));
};

let render = (~context, ~serverState: Store.t, ()) => {
  let store = serverState;
  let serializedState = Store.serializeState(serverState.config);
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let serverPath = UniversalRouterDream.contextPath(context);
  let serverSearch = UniversalRouterDream.contextSearch(context);

  let app =
    <UniversalRouter
      router=Routes.router
      state=store  // Pass state for metadata resolution
      routeRoot
      serverPath
      serverSearch
    />;

  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=app,
      ~routeRoot,
      ~path=serverPath,
      ~search=serverSearch,
      ~serializedState,
      ~state=store,  // Pass state for SSR metadata resolution
      (),
    );

  <Store.Context.Provider value=store>
    document
  </Store.Context.Provider>;
};

let app =
  UniversalRouterDream.app(
    ~router=Routes.router,
    ~getServerState,
    ~render,
    (),
  );
```

## Client-Side Integration

### Hydration Entry Point

```reason
// Index.re
let root = ReactDOM.querySelector("#root") |. Belt.Option.getExn;
let store = Store.hydrateStore();

ReactDOM.hydrateRoot(
  root,
  <Store.Context.Provider value=store>
    <UniversalRouter 
      router=Routes.router 
      state=store  // Pass state for metadata resolution
    />
  </Store.Context.Provider>,
);
```

### Navigation

```reason
// Inside a component
let navigate = UniversalRouter.useNavigate();

// Navigate to a route
navigate(~to_="/about", ());

// Navigate with query params
navigate(~to_="/search", ~search="?q=hello", ());

// Navigate programmatically
let handleClick = () => {
  navigate(~to_="/dashboard", ());
};
```

### Link Components

```reason
// Basic link
<UniversalRouter.Link to_="/about">About</UniversalRouter.Link>

// Link with active state
<UniversalRouter.Link
  to_="/products"
  className={({isActive}) => 
    isActive ? "nav-link active" : "nav-link"
  }>
  Products
</UniversalRouter.Link>
```

## API Reference

### Route Creation

#### `UniversalRouter.create`

```reason
let create: (
  ~document: document,
  ~notFound: module(Page),
  list(routeConfig),
) => router;
```

Creates a router instance with the given configuration.

**Parameters:**
- `~document`: Document configuration for SSR
- `~notFound`: 404 page module
- `list(routeConfig)`: List of route configurations

#### `UniversalRouter.index`

```reason
let index: (
  ~id: string,
  ~page: module(Page),
  unit,
) => routeConfig;
```

Creates an index route (e.g., `/`).

#### `UniversalRouter.route`

```reason
let route: (
  ~id: string,
  ~path: string,
  ~page: module(Page),
  ~resolveTitle: titleResolver=?,
  ~resolveTitleWithState: titleResolverWithState('state)=?,
  ~resolveHeadTags: headTagsResolver=?,
  ~resolveHeadTagsWithState: headTagsResolverWithState('state)=?,
  list(routeConfig),
  unit,
) => routeConfig;
```

Creates a route with the given path pattern.

**Path patterns:**
- Static: `"about"` → `/about`
- Parameter: `"product/:id"` → `/product/123`
- Multiple: `"category/:cat/product/:id"` → `/category/food/product/456`

**Metadata Options:**
- `~resolveTitle`: Function to resolve page title from path/params/query
- `~resolveTitleWithState`: Function to resolve title with access to app state
- `~resolveHeadTags`: Function to resolve meta tags from path/params/query  
- `~resolveHeadTagsWithState`: Function to resolve meta tags with access to app state

**Resolver Types:**
```reason
type titleResolver = (
  ~path: string,
  ~params: Params.t,
  ~query: Query.t,
) => string;

type titleResolverWithState('state) = (
  ~path: string,
  ~params: Params.t,
  ~query: Query.t,
  ~state: 'state,
) => string;

type headTagsResolver = (
  ~path: string,
  ~params: Params.t,
  ~query: Query.t,
) => list(headTag);

type headTagsResolverWithState('state) = (
  ~path: string,
  ~params: Params.t,
  ~query: Query.t,
  ~state: 'state,
) => list(headTag);
```

#### `UniversalRouter.group`

```reason
let group: (
  ~path: string,
  ~layout: module(Layout),
  list(routeConfig),
  unit,
) => routeConfig;
```

Groups routes under a common path prefix and layout.

### Server Context

The `serverContext` type is parameterized by your application state type:

```reason
type serverContext('state) = {
  request: Dream.request,
  routeRoot: string,
  path: string,
  search: string,
  query: UniversalRouter.Query.t,
  params: UniversalRouter.Params.t,
  matchResult: UniversalRouter.matchResult('state),
};
```

#### `UniversalRouterDream.contextRouteRoot`

```reason
let contextRouteRoot: serverContext('state) => string;
```

Gets the route root from the server context.

#### `UniversalRouterDream.contextPath`

```reason
let contextPath: serverContext('state) => string;
```

Gets the current request path.

#### `UniversalRouterDream.contextSearch`

```reason
let contextSearch: serverContext('state) => string;
```

Gets the query string from the request.

## Best Practices

### Route Organization

Keep routes organized by feature:

```
src/
  Routes/
    Public.re       # Public routes (home, about, contact)
    Dashboard.re    # Dashboard routes with auth
    Admin.re        # Admin routes
  Routes.re         # Main router combining all routes
```

### Type-Safe Route IDs

Use a variant for route IDs to prevent typos:

```reason
type routeId =
  | Home
  | About
  | Product(string);

let routeIdToString = (id: routeId) =>
  switch (id) {
  | Home => "home"
  | About => "about"
  | Product(_) => "product"
  };
```

### Lazy Loading

For large applications, consider lazy loading routes:

```reason
// HomePageLazy.re
let%lazy_component make = () => {
  let%module HomePage = dynamic_import("./HomePage.re");
  <HomePage />;
};
```

### SEO and Meta Tags

Customize the document for each route:

```reason
// Page.re
let document = UniversalRouter.document(
  ~title="Product Details | My Store",
  ~meta=[|
    {"name": "description", "content": "View product details"},
    {"property": "og:title", "content": "Product Name"},
  |],
  (),
);
```

## Examples

### E-commerce Routes

See the [ecommerce demo](../demos/ecommerce/ui/src/Routes.re) for a complete example including:
- Product listing with filters
- Product detail pages
- Shopping cart
- Checkout flow
- User authentication

### Blog Routes

```reason
let router =
  UniversalRouter.create(
    ~document=UniversalRouter.document(~title="Blog", ()),
    ~notFound=(module NotFoundPage),
    [
      UniversalRouter.group(
        ~path="blog",
        ~layout=(module BlogLayout),
        [
          UniversalRouter.index(~id="blog-home", ~page=(module BlogHomePage), ()),
          UniversalRouter.route(
            ~id="blog-post",
            ~path=":slug",
            ~page=(module BlogPostPage),
            [],
            (),
          ),
          UniversalRouter.route(
            ~id="blog-category",
            ~path="category/:category",
            ~page=(module BlogCategoryPage),
            [],
            (),
          ),
        ],
        (),
      ),
    ],
  );
```

## Migration Guide

### From React Router

Key differences:
1. Routes defined in Reason/OCaml, not JSX
2. Layouts are explicit route groups
3. Server-side rendering is first-class
4. Type-safe by default

## Troubleshooting

### Routes not matching

- Check that the path pattern matches exactly (including leading `/`)
- Verify route parameters use `:paramName` syntax
- Ensure the router is mounted at the correct base path in Dream

### 404 on all routes

- Verify `Dream.get "/**"` catches all routes
- Check that `notFound` page module is correctly defined
- Ensure routes are registered in the router configuration

### Hydration mismatches

- Ensure server and client render identical route trees
- Check that route parameters are parsed consistently
- Verify no client-only logic runs during SSR

## Related Documentation

- [Dream Router Store Setup](dream-router-store-setup.md) - Complete integration guide
- [Universal Store](universal-reason-react.store.md) - State management integration
- [Dream Framework](https://github.com/aantron/dream) - Web server framework
