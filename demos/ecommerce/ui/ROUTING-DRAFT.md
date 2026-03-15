We need to somehow configure the routing of a Premise. This is a proposal draft.

This is how we will remove the hardcoded premise ID from the application.

In the main Router in App.tsx, we should do a mapping driven by the PostgreSQL database:

## How table routes works:

Root path defined, such as "/"
When requests from this root path come in, the premise ID will be set to the associated value from the routes table.

## Data relationship

Use the same router as in App.tsx and then just exclude the first "root" part of the path from it, and then pass it to the router.
