# universal-reason-react/components

Shared rendering primitives for universal `server-reason-react` apps.

- Provides document-level components used during SSR and hydration.
- Intended to keep the HTML shell consistent between Dream and the browser.
- Current examples include document wrappers and small shared helpers like client-only rendering utilities.

Use this package when you want:

- one document structure for server and client
- reusable SSR-safe UI helpers
- a shared place for rendering concerns that should not live in a single demo app

This package is part of the current prototype stack and its API is still subject to change.
This document is AI-generated and should be reviewed and edited by a human before being considered final.
