# universal-reason-react/router

Shared nested routing for Dream and Reason React applications.

## Overview

The Universal Router allows you to define your routes once and use them in both server-side rendering (Dream) and client-side navigation. This eliminates route duplication and ensures consistency between SSR and client routing.

## Features

- **Single Route Definition**: Define routes once, use everywhere
- **Nested Layouts**: Support for layout composition and nesting
- **Type-Safe Routing**: Compile-time route matching and parameter validation
- **SSR Integration**: Seamless server-side rendering with Dream
- **Client Navigation**: Smooth client-side transitions without page reloads
- **Href Generation**: Type-safe URL construction

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

## Server-Side Integration

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
let getServerState = (context: UniversalRouterDream.serverContext) => {
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  
  // Fetch data based on the current route
  let* data = fetchDataForRoute(routeRoot);
  
  Lwt.return(UniversalRouterDream.State(data));
};

let render = (~context, ~serverState, ()) => {
  let store = Store.createStore(serverState);
  let routeRoot = UniversalRouterDream.contextRouteRoot(context);
  let serverPath = UniversalRouterDream.contextPath(context);
  let serverSearch = UniversalRouterDream.contextSearch(context);

  let app =
    <UniversalRouter
      router=Routes.router
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
      ~serializedState=Store.serializeState(serverState),
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
    <UniversalRouter router=Routes.router />
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
  list(routeConfig),
  unit,
) => routeConfig;
```

Creates a route with the given path pattern.

**Path patterns:**
- Static: `"about"` → `/about`
- Parameter: `"product/:id"` → `/product/123`
- Multiple: `"category/:cat/product/:id"` → `/category/food/product/456`

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

#### `UniversalRouterDream.contextRouteRoot`

```reason
let contextRouteRoot: serverContext => string;
```

Gets the route root from the server context.

#### `UniversalRouterDream.contextPath`

```reason
let contextPath: serverContext => string;
```

Gets the current request path.

#### `UniversalRouterDream.contextSearch`

```reason
let contextSearch: serverContext => string;
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
