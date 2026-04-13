# Documentation Audit Notes

> **Audit Date**: 2026-04-12
> **Scope**: All documentation files in `docs/` directory
> **Status**: Audit complete, fixes pending

## Summary

This audit reviewed all 22 documentation files in the `docs/` directory against actual source code implementations. The audit found varying levels of accuracy across documents, with most requiring at least minor updates and several requiring significant corrections.

### Quick Status Overview

| Priority | Document | Status | Key Issue |
|----------|----------|--------|-----------|
| P0 | API_REFERENCE.md | **Major Issues** | Router/store types wrong, components don't exist |
| P0 | universal-reason-react.store.md | Mostly Accurate | Bootstrap docs overstate server-side creation |
| P0 | universal-reason-react.router.md | **Partial Issues** | Signatures incomplete, missing hooks |
| P0 | realtime-schema.sql-annotations.md | Mostly Accurate | `@table` name ignored by parser |
| P0 | reason-realtime.dream-middleware.md | **Needs Revision** | Missing optional params, wrong handler types |
| P0 | reason-realtime.pgnotify-adapter.md | **Partial Issues** | `t` is concrete not opaque, wrong message format |
| P1 | dream-router-store-setup.md | **Major Issues** | Example uses ecommerce, not todo-multiplayer |
| P1 | store-consistency-model.md | **Accurate** | Minor suggestion for equal-timestamp behavior |
| P1 | realtime-schema.queries.md | Minor Issues | Example snippet shows SQL outside block |
| P1 | realtime-schema.mutations.md | Minor Issues | Wrong example source file referenced |
| P1 | realtime-schema.generated-artifacts.md | **Needs Update** | Migrations directory doesn't exist |
| P1 | realtime.streaming-lifecycle.md | Minor Issues | Missing ping/pong, cross-tab broadcast docs |
| P2 | INSTALLATION.md | **Major Issues** | OCaml version mismatch (5.4.0 vs 5.4.1) |
| P2 | TROUBLESHOOTING.md | Mostly Accurate | Could align with Python wrapper workflow |
| P2 | testing.md | Not Audited | Task returned no output |
| P2 | universal-reason-react.components.md | Not Audited | Task incomplete |
| P2 | universal-reason-react.intl.md | **Accurate** | Minor suggestions for formatToParts example |
| P2 | reason-realtime.stream.md | Minor Issues | llm-chat example doesn't exist in repo |

---

## Detailed Findings by Document

### P0: Critical API Documentation

#### 1. API_REFERENCE.md

**Status**: Major Discrepancies Found

**Issues**:
1. Router types are incorrect - `router`, `routeConfig`, `document` shapes don't match source
2. Store types inaccurate - `store`, `patch` not opaque, `subscription` not exported
3. Components `NoSSR`, `Suspense`, `ErrorBoundary` don't exist in implementation
4. Middleware and Adapter types incomplete/incorrect

**Recommended Fixes**:
- Update router type signatures to match `UniversalRouter.re`
- Correct store type exports based on actual implementation
- Remove non-existent components or add them to codebase
- Update middleware/adapter signatures with correct parameter types

#### 2. universal-reason-react.store.md

**Status**: Mostly Accurate

**Issues**:
1. Bootstrap `withCreatedProvider` misdescribed as server-side (it's client-only)
2. IndexedDB `actions` store created for all DBs, not just synced stores
3. Events section exists in source but not fully documented

**Recommended Fixes**:
- Clarify `withCreatedProvider` is client-side only
- Document that `actions` store is always created
- Add comprehensive Events section

#### 3. universal-reason-react.router.md

**Status**: Partially Inaccurate

**Issues**:
1. `create`, `index`, `route`, `group` signatures incomplete/wrong
2. Missing hooks: `useParams`, `useSearch`, `useMatch`, `useBasePath`
3. Metadata example shows duplicate route IDs (confusing)
4. Missing `RouteNavLink` component documentation

**Recommended Fixes**:
- Update function signatures with all optional parameters
- Document all exported hooks
- Fix metadata example to use single route
- Add `RouteNavLink` to component docs

#### 4. realtime-schema.sql-annotations.md

**Status**: Mostly Accurate

**Issues**:
1. `@table` name is ignored by parser (doc implies required)
2. `@broadcast_parent` example uses wrong query name
3. `@broadcast_to_views` example is synthetic, not from repo
4. `@handler sql` is redundant (default behavior)

**Recommended Fixes**:
- Clarify `@table` is a marker annotation
- Fix example query names to match actual files
- Replace synthetic examples with real ones from repo
- Note that `@handler sql` is optional/default

#### 5. reason-realtime.dream-middleware.md

**Status**: Needs Revision

**Issues**:
1. `Middleware.create` missing optional params (`validate_mutation`, `handle_media`, `handle_disconnect`)
2. `broadcast` missing `?wrap` parameter
3. `Adapter.subscribe` handler type wrong
4. Message protocol incomplete (missing `media`, `error` types)

**Recommended Fixes**:
- Add all optional parameters to `create` signature
- Add `?wrap` to `broadcast`
- Correct `subscribe` handler type
- Complete message protocol documentation

#### 6. reason-realtime.pgnotify-adapter.md

**Status**: Partially Inaccurate

**Issues**:
1. `Pgnotify_adapter.t` is concrete record, not opaque
2. `subscribe` missing `?wrap` argument
3. Message format section describes wrong shape (raw vs transformed)

**Recommended Fixes**:
- Document `t` as concrete type with fields
- Add `?wrap` parameter to `subscribe`
- Rewrite message format to show transformed output

---

### P1: Architecture Guides

#### 7. dream-router-store-setup.md

**Status**: Major Issues

**Issues**:
1. Example uses ecommerce pattern, not todo-multiplayer
2. `CartStore` referenced but doesn't exist in multiplayer demo
3. Hydration pattern shows two stores, actual code uses one

**Recommended Fixes**:
- Add dedicated todo-multiplayer section
- Remove or clarify CartStore references
- Document single-store hydration for todo-multiplayer

#### 8. store-consistency-model.md

**Status**: Accurate ✓

**Minor Suggestions**:
- Add explicit note about equal-timestamp behavior (SSR preferred)

#### 9. realtime-schema.queries.md

**Status**: Minor Issues

**Issues**:
1. Example snippet shows SQL outside block comment (parser expects inside)

**Recommended Fixes**:
- Update snippet to show SQL inside block comment

#### 10. realtime-schema.mutations.md

**Status**: Minor Issues

**Issues**:
1. Example sources reference `inventory.sql` but mutations are in `schema.sql`

**Recommended Fixes**:
- Correct example source references

#### 11. realtime-schema.generated-artifacts.md

**Status**: Fixed

**Issues**:
1. Docs incorrectly implied migrations don't exist - they do (in llm-chat demo)
2. Trigger function names only partially documented

**Fixes Applied**:
- Added note that llm-chat demo uses migrations
- Added trigger function names for both demos
- Added llm-chat to Examples in this repo section

#### 12. realtime.streaming-lifecycle.md

**Status**: Minor Issues

**Issues**:
1. Missing ping/pong handshake documentation
2. Cross-tab `optimistic_action` broadcast not described
3. Ack-timeout/retry semantics not documented

**Recommended Fixes**:
- Add heartbeat frame documentation
- Document cross-tab optimistic broadcast
- Add ack-timeout behavior section

---

### P2: Setup & Troubleshooting

#### 13. INSTALLATION.md

**Status**: Major Issues

**Issues**:
1. OCaml version mismatch: docs say 5.4.0, `dune-project` pins 5.4.1
2. `.envrc.example` doesn't exist (actual file is `.envrc`)
3. Missing environment variables: `LLM_CHAT_DOC_ROOT`, `VIDEO_CHAT_DOC_ROOT`
4. Package manager: docs mention npm, repo uses pnpm
5. No `engines` field in package.json

**Recommended Fixes**:
- Update OCaml version to 5.4.1
- Correct env file instructions
- Add all demo env vars
- Document pnpm as package manager
- Add engines field to package.json

#### 14. TROUBLESHOOTING.md

**Status**: Mostly Accurate

**Minor Suggestions**:
- Align build diagnostic guidance with Python wrapper workflow
- Add LSP cross-reference section

#### 15. testing.md

**Status**: Not Fully Audited

**Note**: Audit task returned no output. Manual review recommended.

#### 16. universal-reason-react.components.md

**Status**: Not Fully Audited

**Note**: Audit task incomplete. Manual review recommended.

#### 17. universal-reason-react.intl.md

**Status**: Accurate ✓

**Minor Suggestions**:
- Add `formatToParts` example in Quick Start

#### 18. reason-realtime.stream.md

**Status**: Minor Issues

**Issues**:
1. llm-chat example referenced but doesn't exist in repo

**Recommended Fixes**:
- Remove or annotate external example reference

---

## New Documentation

### GETTING-STARTED.md ✓

**Status**: Created and Verified

**Content**:
- Step-by-step PostgreSQL-backed realtime app tutorial
- SQL schema with annotations
- Synced CRUD store configuration
- Dream server setup
- Universal routing
- Build verification passed

**Cross-links**: Added to docs/README.md

---

## Priority Fix Order

1. **P0 - API Reference** - Most critical, affects all developers
2. **P0 - Router** - Core API documentation
3. **P0 - Middleware/Adapter** - Realtime integration
4. **P2 - Installation** - New user onboarding
5. **P1 - Dream Router Store Setup** - Integration guide
6. **P1 - Generated Artifacts** - Build understanding

---

## Verification Results

### Build Verification
```
✓ python3 scripts/run_dune.py build @all-apps
  - todo/ui: Index.re.js 454.6kb, Index.re.css 1.5kb
  - ecommerce/ui: Index.re.js 586.0kb, Index.re.css 311.2kb
  
✓ python3 scripts/run_dune.py build demos/todo-multiplayer/server/src/server.exe
  - server.exe: 29.5M
```

### Documentation Cross-links
```
✓ GETTING-STARTED.md linked from docs/README.md
```

---

## Next Steps

1. Apply fixes to P0 documents (6 files)
2. Apply fixes to P1 documents (5 files)
3. Apply fixes to P2 documents (3 files)
4. Complete audit of testing.md and components.md
5. Update all cross-references after fixes
6. Re-run verification build
