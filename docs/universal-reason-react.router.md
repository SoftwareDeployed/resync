# universal-reason-react/router

Shared nested routing for Dream and Reason React.

- Define routes once and consume them from both the browser and the Dream server.
- Supports nested layouts, typed route matching, href generation, document rendering, and SSR entrypoints.
- Pairs with `UniversalRouterDream` on the server and `UniversalRouter` on the client.

Use this package when you want:

- one route tree for SSR and client navigation
- layout-aware routing without duplicating route definitions
- a root-level `getServerState` flow for initial SSR bootstrap

The ecommerce demo is the current reference for how to structure routes, SSR rendering, and Dream integration.

This package is part of the current prototype stack and its API is still subject to change.
This document is AI-generated and should be reviewed and edited by a human before being considered final.
