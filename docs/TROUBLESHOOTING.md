# Troubleshooting Guide

Common issues and solutions for Universal Reason React applications.

## Build Issues

### dune: command not found

**Problem:** `dune` command not available in shell

**Solution:**
```bash
# Install dune via opam
opam install dune

# Ensure opam environment is loaded
eval $(opam env)

# Or add to shell profile
echo 'eval $(opam env)' >> ~/.bashrc
```

### Compilation errors in .re files

**Problem:** Syntax errors or type mismatches

**Solutions:**

1. **Check Reason syntax:**
```reason
// Ensure proper semicolons
let x = 5;
let y = 10;

// Ensure proper braces
let make = () => {
  <div>{React.string("Hello")}</div>
};
```

2. **Verify file extension:**
- Reason files: `.re`
- OCaml files: `.ml`
- Interface files: `.rei` or `.mli`

3. **Check dune configuration:**
```lisp
; Ensure correct preprocessor for Reason
(preprocess (pps melange.ppx reactjs-jsx-ppx))
```

### Module not found

**Problem:** `Unbound module X`

**Solutions:**

1. **Check library name:**
```lisp
; In dune file
(libraries
 universal_reason_react_router_js  ; Correct
 universal-reason-react-router-js) ; Wrong (hyphens)
```

2. **Verify dependency is installed:**
```bash
opam list | grep universal-reason-react

# Install if missing
opam install universal-reason-react-router
```

3. **Check for circular dependencies:**
```bash
# Look for cycles in dune files
grep -r "libraries" --include="dune" .
```

### opam dependency conflicts

**Problem:** Package conflicts during installation

**Solutions:**

1. **Update opam:**
```bash
opam update
opam upgrade
```

2. **Check OCaml version:**
```bash
ocaml -version  # Should be 4.14.0 or later
opam switch  # Verify active switch
```

3. **Clean and reinstall:**
```bash
opam switch remove .
opam switch create 4.14.1 --deps-only
```

## Runtime Issues

### Hydration mismatch

**Problem:** Server and client render different HTML

**Error:**
```
Warning: Text content did not match. Server: "Server" Client: "Client"
```

**Solutions:**

1. **Check for environment-specific code:**
```reason
// ❌ Bad: Different values on server/client
let time = Js.Date.now();

// ✅ Good: Use effect for client-only values
let (time, setTime) = React.useState(() => "");
React.useEffect0(() => {
  setTime(_ => Js.Date.now() |> string_of_float);
  None;
});
```

2. **Use NoSSR wrapper:**
```reason
<UniversalComponents.NoSSR>
  <ClientOnlyComponent />
</UniversalComponents.NoSSR>
```

3. **Check document structure:**
```reason
// Server and client must render identical structure
<Document>
  <div id="root">
    <App />
  </div>
</Document>
```

### Store not hydrating

**Problem:** Client-side store is empty after hydration

**Solutions:**

1. **Verify state element exists:**
```html
<!-- Check HTML output -->
<script id="__store_state__" type="application/json">
  {"items": [...], "user": null}
</script>
```

2. **Check stateElementId matches:**
```reason
// Store.re
let stateElementId = "__store_state__";  // Must match HTML id
```

3. **Verify serialization:**
```reason
// Server-side
let serializedState = Store.serializeState(serverState);
// Check: print_endline(serializedState);
```

4. **Check for JSON parsing errors:**
```javascript
// In browser console
document.getElementById('__store_state__').textContent
JSON.parse(document.getElementById('__store_state__').textContent)
```

### Real-time updates not working

**Problem:** WebSocket connected but no updates received

**Solutions:**

1. **Check WebSocket connection:**
```javascript
// Browser console
const ws = new WebSocket('ws://localhost:8080/_events');
ws.onopen = () => console.log('Connected');
ws.onmessage = (e) => console.log('Message:', e.data);
```

2. **Verify PostgreSQL triggers:**
```sql
-- Check triggers exist
SELECT * FROM pg_trigger WHERE tgname LIKE '%changes%';

-- Test notification manually
SELECT pg_notify('items_changes', '{"test": true}');
```

3. **Check subscription encoding:**
```reason
// Verify subscriptionOfConfig returns correct structure
let subscriptionOfConfig = (config) => {
  config.user |> Option.map(user => {
    userId: user.id  // Must match server expectation
  });
};
```

4. **Enable debug logging:**
```reason
// server.ml
Dream.logger @@ Dream.run(...)
```

### Database connection errors

**Problem:** Can't connect to PostgreSQL

**Error:**
```
Caqti_error: Postgresql connection failed
```

**Solutions:**

1. **Verify connection string:**
```bash
export DB_URL="postgres://user:password@localhost:5432/dbname"
# Test connection
psql $DB_URL -c "SELECT 1"
```

2. **Check PostgreSQL is running:**
```bash
pg_isready -h localhost -p 5432

# Start PostgreSQL
# macOS: brew services start postgresql
# Ubuntu: sudo service postgresql start
```

3. **Verify permissions:**
```sql
-- Grant permissions
GRANT ALL PRIVILEGES ON DATABASE myapp TO myapp_user;
GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO myapp_user;
```

### Router 404 errors

**Problem:** All routes return 404

**Solutions:**

1. **Check Dream route order:**
```reason
Dream.router([
  // Static routes first
  Dream.get "/static/**" (Dream.static doc_root),
  
  // Then catch-all
  Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app),
]);
```

2. **Verify router configuration:**
```reason
// Routes.re
let router =
  UniversalRouter.create(
    ~document=UniversalRouter.document(~title="My App", ()),
    ~notFound=(module NotFoundPage),  // Required
    [
      UniversalRouter.index(~id="home", ~page=(module HomePage), ()),
      // ... more routes
    ],
  );
```

3. **Check notFound page module:**
```reason
// NotFoundPage.re
[@react.component]
let make = () => {
  <div>{React.string("404 - Page not found")}</div>;
};
```

## Development Issues

### Merlin/autocomplete not working

**Problem:** No IDE support or autocomplete

**Solutions:**

1. **Install merlin:**
```bash
opam install merlin
```

2. **Generate .merlin files:**
```bash
dune build @check
```

3. **VS Code specific:**
```json
// .vscode/settings.json
{
  "ocaml.sandbox": {
    "kind": "opam",
    "switch": "${workspaceFolder}"
  }
}
```

4. **Restart language server:**
- VS Code: `Cmd/Ctrl + Shift + P` → "OCaml: Restart language server"
- Emacs: `M-x merlin-restart-process`

### Hot reload not working

**Problem:** Changes don't reflect without manual rebuild

**Solutions:**

1. **Use watch mode:**
```bash
# Terminal 1: Watch and rebuild
dune build -w

# Terminal 2: Run server
dune exec --watch ./server/bin/main.exe
```

2. **Clear build cache:**
```bash
dune clean
dune build
```

3. **Check for syntax errors:**
```bash
# Prevents rebuild on errors
dune build -w 2>&1 | head -20
```

### Slow build times

**Problem:** Builds take too long

**Solutions:**

1. **Enable parallel builds:**
```bash
dune build -j 4
```

2. **Use release profile:**
```bash
dune build --profile release
```

3. **Build only changed files:**
```bash
dune build @check  # Type checking only
dune build ./ui    # Specific target
```

4. **Check for unnecessary dependencies:**
```lisp
; In dune files, remove unused libraries
(libraries
  ; Only include what you need
  universal_reason_react_router_js
  reason-react)
```

## Testing Issues

### Tests fail with Lwt errors

**Problem:** Async test failures

**Solutions:**

1. **Proper Lwt handling:**
```reason
// In test
let%lwt result = Database.getUser("123");
Alcotest.(check (option user)) "user exists" (Some(expected)) result;
Lwt.return_unit;
```

2. **Use Alcotest_lwt:**
```lisp
; dune
test
 (name test_suite)
 (libraries alcotest alcotest-lwt lwt))
```

### Database tests fail

**Problem:** Database not available in test environment

**Solutions:**

1. **Use test database:**
```bash
export TEST_DB_URL="postgres://localhost:5432/myapp_test"
```

2. **Setup/teardown in tests:**
```reason
let test_setup = () => {
  let* () = Database.truncate_all();
  let* () = Database.seed_test_data();
  Lwt.return_unit;
};
```

## Performance Issues

### Slow initial page load

**Problem:** First render takes too long

**Solutions:**

1. **Reduce initial state size:**
```reason
// Only send necessary data
let getServerState = (context) => {
  let* items = Database.getItems(~limit=20);  // Paginate
  Lwt.return(State({items}));
};
```

2. **Enable compression:**
```reason
Dream.run
@@ Dream.logger
@@ Dream.memory_sessions
@@ Dream.compress  // Add compression
@@ router
```

3. **Optimize static assets:**
```bash
# Minify JavaScript
npx esbuild ui/src/Index.bs.js --minify --outfile=static/app.js
```

### Memory leaks

**Problem:** Server memory grows over time

**Solutions:**

1. **Limit WebSocket connections:**
```reason
let config = {
  ...defaultConfig,
  maxConnections: Some(5),  // Per user
};
```

2. **Clean up resources:**
```reason
React.useEffect(() => {
  let subscription = Store.subscribe(callback);
  Some(() => Store.unsubscribe(subscription));
}, [||]);
```

3. **Monitor with tools:**
```bash
# OCaml memory stats
OCAMLRUNPARAM="s=1G" dune exec ./server/bin/main.exe
```

## Common Error Messages

### "Unbound value make"

**Cause:** Component not properly exported or wrong module name

**Fix:**
```reason
// Ensure component is named 'make'
[@react.component]
let make = () => {
  <div />
};
```

### "This expression has type..."

**Cause:** Type mismatch

**Fix:**
```reason
// Add type annotation
let items: list(Item.t) = store.items;

// Or use type coercion
let id: string = (item.id :> string);
```

### "The constructor X does not belong to type Y"

**Cause:** Wrong variant constructor

**Fix:**
```reason
type status =
  | Loading
  | Loaded
  | Error;

// Correct usage
let state = Loaded(data);

// Not: let state = Success(data);
```

### "The record field x is undefined"

**Cause:** Field doesn't exist in record type

**Fix:**
```reason
type user = {
  name: string,
  email: string,
};

// Access existing fields
let name = user.name;

// Can't access non-existent fields
// let age = user.age;  // Error!
```

## Getting More Help

### Enable Debug Logging

```reason
// server.ml
let () =
  Dream.run ~debug:true  // Enable debug mode
  @@ Dream.logger
  @@ router
```

### Check Logs

```bash
# Application logs
LOG_LEVEL=debug dune exec ./server/bin/main.exe

# PostgreSQL logs
# macOS: tail -f /usr/local/var/log/postgresql.log
# Ubuntu: tail -f /var/log/postgresql/postgresql-*.log
```

### Diagnostic Commands

```bash
# Check environment
echo $DB_URL
echo $API_BASE_URL
dune --version
ocaml -version

# Verify build
dune build @check 2>&1 | head -50

# List opam packages
opam list | grep universal-reason-react
```

### Still Stuck?

1. Check [GitHub Issues](https://github.com/anomalyco/opencode/issues)
2. Review the [ecommerce demo](../demos/ecommerce/) for working examples
3. Consult the [Reason documentation](https://reasonml.github.io/)
4. Ask in [Discord/Discussions](https://github.com/anomalyco/opencode/discussions)
