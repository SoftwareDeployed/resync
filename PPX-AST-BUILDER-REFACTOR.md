# PPX Codegen Refactor: sprintf → Ast_builder

## Current State

The `Realtime_schema_codegen.ml` uses `Printf.sprintf` extensively to generate OCaml source code as strings. These strings are then:

1. **In PPX**: Parsed back into AST via `Parse.implementation` in `Realtime_schema_ppx.ml`
2. **Standalone**: Written directly to `.ml` files

Example current pattern:
```ocaml
let query_module_declaration tables (query : query) =
  Printf.sprintf
    "module %s = struct\n let name = %S\n let sql = %S\n ..."
    (module_name_of_table query.name) query.name query.sql
```

## Problem with sprintf

1. **Round-trip parsing**: PPX generates strings, then parses them back to AST - wasteful
2. **No type safety**: Generated code isn't validated until parse time
3. **Hard to maintain**: Complex string interpolation for nested structures
4. **Error prone**: Easy to make syntax mistakes in format strings

## Solution: Ast_builder

Use `Ppxlib.Ast_builder.Default` to construct AST directly:

```ocaml
open Ppxlib
open Ast_builder.Default

(* Instead of: *)
(* Printf.sprintf "module %s = struct\n let name = %S\n ..." name value *)

(* Do: *)
pexp_structure ~loc [
  pstr_module ~loc (module_binding ~loc 
    ~name:(Located.mk ~loc (Some name))
    ~body:(pmod_structure ~loc [
      pstr_eval ~loc (eapply ~loc (evar ~loc "name") [estring ~loc value])
    ])
  )
]
```

## Files to Modify

1. **`packages/realtime-schema/src/Realtime_schema_codegen.ml`**
   - Replace string-returning functions with AST-building functions
   - Use `Ast_builder.Default` for all node construction
   - Return `Ppxlib.Ast.structure` (list of structure items) instead of `string`

2. **`packages/realtime-schema/src/Realtime_schema_ppx.ml`**
   - Remove the string parsing step
   - Use generated AST directly

3. **`packages/realtime-schema/src/Realtime_schema_codegen_main.ml`**
   - If still writing files, use `Pprintast` to convert AST back to string
   - Or write AST directly if dune supports it

## Key Ast_builder Patterns

```ocaml
(* Basic values *)
estring ~loc "hello"        (* "hello" *)
eint ~loc 42               (* 42 *)
eident ~loc (Lident "x")   (* x *)
eunit ~loc                 (* () *)

(* Application *)
eapply ~loc (evar ~loc "f") [earg1; earg2]  (* f arg1 arg2 *)

(* Record *)
precord ~loc [ Lident "field", eint ~loc 1 ]  (* { field = 1 } *)

(* Let binding *)
pexp_let ~loc Nonrecursive [
  pvb_pat ~loc (pvar ~loc "x")
    ~expr:(eint ~loc 1)
] ~body:(evar ~loc "x")

(* Match *)
pexp_match ~loc (evar ~loc "x") [
  ppat_construct ~loc (Lident "Some") (Some p) -> body
]

(* Module *)
pmod_structure ~loc [ pstr_eval ~loc expr ]
```

## Implementation Strategy

1. **Phase 1**: Create new AST-building functions alongside existing string functions
2. **Phase 2**: Update PPX to use AST functions
3. **Phase 3**: Update standalone generator to use `Pprintast.string_of_structure`
4. **Phase 4**: Remove old string functions

## References

- [ppxlib Ast_builder documentation](https://ppxlib.readthedocs.io/en/stable/api/Ast_builder.html)
- [Ppxlib Metaquot](https://ppxlib.readthedocs.io/en/stable/api/Ppxlib_metaquot.html) - quasiquotation alternative
- Example PPX using Ast_builder: `ppx_deriving`, `ppx_sexp_conv`
