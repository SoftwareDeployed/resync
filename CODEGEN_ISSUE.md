# PPX Codegen Issue: params Type Shadowing

## Problem

The PPX codegen in `packages/realtime-schema/src/Realtime_schema_codegen.ml` generates code that causes a type error:

```
Error: The value p has type params but an expression was expected of type string
```

## Root Cause

The generated code has a naming conflict:

1. **Metadata field**: `let params = [...]` - a list of query parameter metadata
2. **Type definition**: `type params = { field: type }` - the actual parameter record type

In OCaml, when a variable and a type have the same name in the same scope, the variable shadows the type. So when the type checker sees:

```ocaml
let paramsHash (p : params) = p.field
```

It resolves `params` to the variable (a list), not the type, causing the error.

## Generated Code Structure

```ocaml
module GetList = struct
  let name = "get_list"
  let sql = "..."
  let params = [(1, "string", "uuid")]  // <-- VARIABLE shadows TYPE
  let return_table = Some "todos"
  ...
  type params = { list_id: string }     // <-- TYPE definition
  let channel (p : params) = p.list_id  // <-- ERROR: params is the variable, not the type
  let paramsHash (p : params) = p.list_id
  ...
end
```

## Attempted Fixes

1. **Renamed function parameters** from `params` to `param_values` (lines 390-397, 536-539) - This fixed the function parameter shadowing but not the metadata field shadowing.

2. **Renamed metadata field** from `params` to `params_metadata`:
   - Changed `let params = [...]` to `let params_metadata = [...]` in module struct (line 417)
   - Changed type definition fields from `params : query_param list` to `params_metadata : query_param list` (lines 131, 139, 1160)
   - Changed record literals to use `params_metadata = ...` instead of `params = ...`

3. **Issue with type name**: The `QueryModule` signature requires `type params`, so the inner type must be named `params`. But the metadata field `let params = [...]` shadows it.

## Current State

After the fixes, there's still an issue with string escaping in the codegen. The line:

```ocaml
Printf.sprintf "type params = {\\\\n%s\\\\n}" fields
```

Has too many backslashes, causing an "Illegal character" error.

## Solution Needed

The fix needs to:

1. Keep the inner type named `params` (required by `QueryModule` signature)
2. Rename the metadata field to something else (e.g., `params_metadata` or `param_info`)
3. Update the type definition to use the new field name
4. Fix the string escaping issue

## Latest Build Error

```
File "demos/todo-multiplayer/shared/native/RealtimeSchema.ml", line 215, characters 19-31:
Error: Unbound type constructor query_params
Hint: Did you mean query_param?
```

This error indicates that somewhere in the generated code, `query_params` is used as a type name but it doesn't exist. This is likely in the `encodeParams` function signature which was changed to use `query_params` instead of `params`:

```ocaml
let encodeParams (p: query_params) = ...  // Should be: let encodeParams (p: params) = ...
```

The type annotation in `encodeParams` and `paramsHash` functions need to use `params` (not `query_params`) to match the `QueryModule` signature.

## Files Modified

- `packages/realtime-schema/src/Realtime_schema_codegen.ml`
  - Line 131: Changed `params = %s` to `params_metadata = %s` in query_record_literal
  - Line 139: Changed `params = %s` to `params_metadata = %s` in mutation_record_literal
  - Line 214: Changed `type query_params = unit` to `type params = unit`
  - Line 390-397: Changed function parameter `params` to `param_values`
  - Line 536-539: Changed function parameter `params` to `param_values`
  - Line 1160: Changed type field `params : query_param list` to `params_metadata : query_param list`

## Related Files

- `packages/universal-reason-react/store/js/QueryRegistryTypes.re` - Defines `QueryModule` signature requiring `type params`
- `demos/*/shared/native/RealtimeSchema.ml` - Generated files with the error
