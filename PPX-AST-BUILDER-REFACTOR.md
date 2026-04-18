# PPX Codegen Refactor: sprintf → Ast_builder

## Status: ✅ COMPLETE

This refactor has been completed. See `PLAN-PPX-AST-BUILDER.md` for the overall project status.

---

## What Was Done

### Completed Changes

1. **`packages/realtime-schema/src/Realtime_schema_codegen.ml`**
   - ✅ Replaced string-returning functions with AST-building functions
   - ✅ Used `Ast_builder.Default` for all node construction
   - ✅ Returns `Ppxlib.Ast.structure` (list of structure items) instead of `string`
   - ✅ Removed dead legacy string codegen path (~33 helpers, net -666 lines)

2. **`packages/realtime-schema/src/Realtime_schema_ppx.ml`**
   - ✅ Removed the string parsing step (`Parse.implementation`)
   - ✅ Uses generated AST directly

3. **`packages/realtime-schema/src/Realtime_schema_codegen_main.ml`**
   - ✅ Uses `Pprintast.string_of_structure` to convert AST back to string for file output

### Key Commits

- `28a65fe` - refactor(realtime-schema): convert codegen to Ast_builder and update PPX
- `65e948b` - feat(realtime-schema): add Ast_builder helpers and fix ppxlib 0.38.0 API
- `71d72cc` - refactor(realtime-schema): WIP AST builder refactor and generated action types (partial)
- `12ef085` - feat(realtime-schema): generate action JSON codecs and remove dead string path

### Before/After

**Before** (string generation + parse round-trip):
```ocaml
let query_module_declaration tables (query : query) =
  Printf.sprintf
    "module %s = struct\n let name = %S\n let sql = %S\n ..."
    (module_name_of_table query.name) query.name query.sql

(* In PPX: *)
let ast = Parse.implementation (Lexing.from_string generated_string)
```

**After** (direct AST construction):
```ocaml
open Ppxlib
open Ast_builder.Default

let query_module_declaration ~loc tables (query : query) =
  pstr_module ~loc (module_binding ~loc
    ~name:(Located.mk ~loc (Some (module_name_of_table query.name)))
    ~expr:(pmod_structure ~loc [
      pstr_value ~loc Nonrecursive [
        value_binding ~loc
          ~pat:(pvar ~loc "name")
          ~expr:(estring ~loc query.name)
      ];
      (* ... *)
    ])
  )

(* In PPX: use AST directly, no parsing needed *)
```

---

## Generated Artifacts

The refactor enables generation of:

1. **Query modules** (existing): `RealtimeSchema.Queries.*` with `caqti_type`, `collect`, `find_opt`
2. **Mutation modules** (existing): `RealtimeSchema.Mutations.*` with `caqti_request`, `param_type`, `exec`
3. **Action types** (new): `RealtimeSchema.MutationActions.action` variant with payload types
4. **JSON codecs** (new): `action_to_json` / `action_of_json` for websocket serialization

---

## Impact

- `demos/todo-multiplayer/ui/src/TodoStore.re`: Reduced from ~360 lines to **82 lines**
- `demos/todo-multiplayer/ui/src/TodoMutations.re`: **Deleted** (zero references)
- Build targets: All pass (`@all-apps @all-servers`)

---

## References

- [ppxlib Ast_builder documentation](https://ppxlib.readthedocs.io/en/stable/api/Ast_builder.html)
- [Ppxlib Metaquot](https://ppxlib.readthedocs.io/en/stable/api/Ppxlib_metaquot.html) - quasiquotation alternative
- `PLAN-PPX-AST-BUILDER.md` - Overall project plan and current status
