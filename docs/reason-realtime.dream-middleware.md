# reason-realtime/dream-middleware

Dream middleware and websocket plumbing for realtime updates.

- Exposes the Dream endpoint used by the client realtime stack.
- Provides the middleware surface that adapters can plug into.
- Intended to be paired with `universal-reason-react/store` for live store updates after initial SSR.

Use this package when you want:

- a Dream websocket endpoint for server-to-client events
- reusable realtime middleware instead of app-specific websocket code

This package is part of the current prototype stack and its API is still subject to change.
This document is AI-generated and should be reviewed and edited by a human before being considered final.
