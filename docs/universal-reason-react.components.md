# universal-reason-react/components

Shared rendering primitives for universal `server-reason-react` applications.

## Overview

This package provides document-level components and rendering utilities that work consistently across server-side rendering (Dream) and client-side hydration. It ensures your HTML shell is identical on both server and client, preventing hydration mismatches.

## Features

- Document wrapper components for consistent HTML structure
- SSR-safe rendering utilities
- Client-only rendering helpers
- Meta tag management
- Script and style injection

## Installation

Add to your `dune` file:

```lisp
(libraries
  universal_reason_react_components_native  ; For server
  universal_reason_react_components_js      ; For client
  server-reason-react.react                 ; Server components
  reason-react)                             ; Client components
```

## Quick Start

### Document Wrapper

```reason
// Document.re
[@react.component]
let make = (~title, ~children, ~serializedState=?) => {
  <html lang="en">
    <head>
      <meta charset="UTF-8" />
      <meta name="viewport" content="width=device-width, initial-scale=1.0" />
      <title> {React.string(title)} </title>
      <link rel="stylesheet" href="/style.css" />
    </head>
    <body>
      <div id="root"> children </div>
      
      {switch (serializedState) {
       | Some(state) =>
         <script
           id="__store_state__"
           type="application/json"
           dangerouslySetInnerHTML={{"__html": state}}
         />
       | None => React.null
       }}
      
      <script src="/app.js" defer=true />
    </body>
  </html>;
};
```

### Client-Only Components

```reason
// components/ClientOnly.re
[@react.component]
let make = (~children) => {
  let (isClient, setIsClient) = React.useState(() => false);
  
  React.useEffect0(() => {
    setIsClient(_ => true);
    None;
  });
  
  if (isClient) {
    children;
  } else {
    React.null;
  };
};

// Usage
<ClientOnly>
  <BrowserOnlyComponent />
</ClientOnly>
```

### Lazy Hydration

```reason
// components/LazyHydrate.re
[@react.component]
let make = (~children, ~delay=100) => {
  let (shouldHydrate, setShouldHydrate) = React.useState(() => false);
  
  React.useEffect0(() => {
    let timeoutId = Js.Global.setTimeout(() => {
      setShouldHydrate(_ => true);
    }, delay);
    
    Some(() => Js.Global.clearTimeout(timeoutId));
  });
  
  <div data-lazy-hydrate={!shouldHydrate ? "true" : "false"}>
    children
  </div>;
};
```

## Components

### Document

The main document wrapper that provides consistent HTML structure.

```reason
<UniversalComponents.Document
  title="My App"
  meta=[|
    {"name": "description", "content": "My app description"},
    {"property": "og:title", "content": "My App"},
  |]
  links=[|
    {"rel": "stylesheet", "href": "/app.css"},
  |]
  scripts=[|
    {"src": "/analytics.js", "async": true},
  |]>
  <App />
</UniversalComponents.Document>
```

### Head

Manage document head elements.

```reason
<UniversalComponents.Head>
  <title> {React.string("Page Title")} </title>
  <meta name="description" content="Page description" />
</UniversalComponents.Head>
```

### NoSSR

Content that should only render on the client.

```reason
<UniversalComponents.NoSSR>
  <ThirdPartyWidget />
</UniversalComponents.NoSSR>
```

### Suspense Boundary

Universal Suspense wrapper.

```reason
<UniversalComponents.Suspense fallback={<LoadingSpinner />}>
  <AsyncComponent />
</UniversalComponents.Suspense>
```

### Error Boundary

Catch and handle rendering errors.

```reason
<UniversalComponents.ErrorBoundary
  fallback={error => <ErrorDisplay message={error.message} />}>
  <ComponentThatMightError />
</UniversalComponents.ErrorBoundary>
```

## Utilities

### isServer / isClient

```reason
let isServer: unit => bool;
let isClient: unit => bool;
```

Check runtime environment.

```reason
let component = () => {
  if (UniversalComponents.isServer()) {
    <ServerOnlyView />;
  } else {
    <ClientView />;
  };
};
```

### useIsClient

Hook for client detection.

```reason
let useIsClient: unit => bool;
```

```reason
[@react.component]
let make = () => {
  let isClient = UniversalComponents.useIsClient();
  
  if (isClient) {
    <ClientOnlyMap />;
  } else {
    <StaticMapPlaceholder />;
  };
};
```

### useHydrated

Track hydration status.

```reason
let useHydrated: unit => bool;
```

```reason
[@react.component]
let make = () => {
  let isHydrated = UniversalComponents.useHydrated();
  
  <button
    disabled={!isHydrated}
    onClick={handleClick}>
    {React.string(isHydrated ? "Click me" : "Loading...")}
  </button>;
};
```

## Advanced Patterns

### Conditional Scripts

```reason
[@react.component]
let make = (~analyticsId=?) => {
  <UniversalComponents.Document title="My App">
    <App />
    
    {switch (analyticsId) {
     | Some(id) =>
       <script
         dangerouslySetInnerHTML={{
           "__html": {j|
             window.analyticsId = "$id";
           |j}
         }}
       />
     | None => React.null
     }}
  </UniversalComponents.Document>;
};
```

### Dynamic Meta Tags

```reason
[@react.component]
let make = (~product) => {
  <>
    <UniversalComponents.Head>
      <title> {React.string(product.name ++ " | Store")} </title>
      <meta name="description" content={product.description} />
      <meta property="og:image" content={product.imageUrl} />
    </UniversalComponents.Head>
    
    <ProductDetails product />
  </>;
};
```

### Progressive Enhancement

```reason
[@react.component]
let make = () => {
  let (isEnhanced, setIsEnhanced) = React.useState(() => false);
  
  React.useEffect0(() => {
    setIsEnhanced(_ => true);
    None;
  });
  
  <form>
    <input type_="text" name="search" />
    <button type_="submit">
      {React.string("Search")}
    </button>
    
    {isEnhanced
      ? <AutocompleteSearch />
      : React.null}
  </form>;
};
```

## Best Practices

### 1. Keep Document Structure Consistent

Server and client must render identical document structures:

✅ **Good:**
```reason
let document =
  <Document title="My App">
    <div id="root">
      <App />
    </div>
  </Document>;
```

❌ **Bad:**
```reason
// Server
<Document title="My App">
  <div id="root"> <App /> </div>
</Document>;

// Client (different!)
<div id="root"> <App /> </div>
```

### 2. Use NoSSR for Browser-Only APIs

Wrap components that use browser APIs:

```reason
<NoSSR>
  <CanvasDrawing />
</NoSSR>
```

### 3. Handle window/document Safely

```reason
let windowWidth = () => {
  if (UniversalComponents.isClient()) {
    Some(Webapi.Dom.window |> Webapi.Dom.Window.innerWidth);
  } else {
    None;
  };
};
```

### 4. Avoid Hydration Mismatches

Don't use random values during SSR:

❌ **Bad:**
```reason
let id = Random.int(100000) |> string_of_int;
```

✅ **Good:**
```reason
let id = React.useMemo(() =>
  Random.int(100000) |> string_of_int,
  [||]
);
```

## Common Pitfalls

### Hydration Mismatch Due to Time

❌ **Bad:**
```reason
let currentTime = Js.Date.now() |> string_of_float;
```

✅ **Good:**
```reason
let (currentTime, setCurrentTime) = React.useState(() => "");

React.useEffect0(() => {
  setCurrentTime(_ => Js.Date.now() |> string_of_float);
  None;
});
```

### Hydration Mismatch Due to User Agent

❌ **Bad:**
```reason
let isMobile = Webapi.Dom.window |> Webapi.Dom.Window.navigator |> /* check user agent */;
```

✅ **Good:**
```reason
let (isMobile, setIsMobile) = React.useState(() => false);

React.useEffect0(() => {
  setIsMobile(_ => checkUserAgent());
  None;
});
```

## Integration with Router

```reason
// EntryServer.re
let render = (~context, ~serverState, ()) => {
  let document =
    UniversalRouter.renderDocument(
      ~router=Routes.router,
      ~children=<App />,
      ~routeRoot,
      ~path=serverPath,
      ~search=serverSearch,
      ~serializedState,
      (),
    );
    
  <UniversalComponents.Document title="My App">
    document
  </UniversalComponents.Document>;
};
```

## Examples

### Complete Page Component

```reason
// pages/ProductPage.re
[@react.component]
let make = (~productId) => {
  let product = Store.useSelector(store =>
    store.products |> List.find(p => p.id == productId)
  );
  
  <>
    <UniversalComponents.Head>
      <title> {React.string(product.name)} </title>
      <meta name="description" content={product.description} />
    </UniversalComponents.Head>
    
    <UniversalComponents.ErrorBoundary
      fallback={_ => <ErrorMessage message="Failed to load product" />}>
      <ProductLayout>
        <ProductHero product />
        <ProductDetails product />
        
        <UniversalComponents.NoSSR>
          <ProductReviews productId />
        </UniversalComponents.NoSSR>
      </ProductLayout>
    </UniversalComponents.ErrorBoundary>
  </>;
};
```

### Layout Component

```reason
// components/Layout.re
[@react.component]
let make = (~children) => {
  <>
    <Header />
    <main className="main-content">
      <UniversalComponents.Suspense
        fallback={<PageLoader />}>
        children
      </UniversalComponents.Suspense>
    </main>
    <Footer />
  </>;
};
```

## Related Documentation

- [universal-reason-react/router](universal-reason-react.router.md) - Routing integration
- [universal-reason-react/store](universal-reason-react.store.md) - State hydration
- [server-reason-react](https://github.com/ml-in-barcelona/server-reason-react) - Server-side React
