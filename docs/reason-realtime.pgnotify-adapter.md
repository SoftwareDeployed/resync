# reason-realtime/pgnotify-adapter

PostgreSQL `LISTEN/NOTIFY` adapter for the realtime middleware layer.

- Connects database notifications to the Dream realtime middleware.
- Useful when store updates should be triggered by Postgres events instead of polling.

Use this package when you want:

- a database-driven realtime event source
- a straightforward way to feed websocket updates from Postgres into the client store

This package is part of the current prototype stack and its API is still subject to change.
This document is AI-generated and should be reviewed and edited by a human before being considered final.
