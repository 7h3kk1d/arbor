# Decisions — p1-arithmetic

Prototype-specific decision log. Lightweight ADR format.

---

## 2026-04-21 — Tech stack follows Hazel where practical

OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.16, Menhir, ppx_deriving, alcotest + qcheck for tests. Plus digestif (not in Hazel) for cryptographic hashing.

**Rationale.** Keeps the door open to Hazel reuse in later prototypes without forcing integration in Phase 1. Reason + dune is a familiar, well-understood stack. digestif gives us BLAKE3 in a modern, idiomatic way.

**Alternatives considered.** Built-in `Digest` (MD5) instead of digestif — simpler but weak; we want the real content-addressing model from day one. A non-Hazel-adjacent stack (Rust, Haskell, TS) — rejected because it would prejudice the long-term Hazel-reuse question with a signal we haven't earned yet.

---

## 2026-04-21 — In-memory only, no persistence

The REPL's Store is a process-local `Hashtbl.t`. Exiting the REPL discards all state.

**Rationale.** Phase 1 validates the core content-addressing model — register, lookup, evaluate, canonicalize. Persistence adds serialization format choices and on-disk layout concerns that don't serve the thesis. Add in a later prototype if it earns its keep.

**Alternatives considered.** A stateful CLI with per-invocation persistence to disk (originally proposed). Rejected as premature machinery.

---

## 2026-04-21 — Interactive REPL as the only interface

Users interact via an interactive prompt. Bare expressions register and evaluate in one step; `:`-prefixed commands operate on the existing Store.

**Rationale.** Lowest-friction way to poke at the system and form honest opinions about it. Matches the "feel" question at the heart of Phase 1.

**Alternatives considered.** Batch runner (file in, results out) — harder to probe interactively. Stateful subcommand CLI — more structured but adds persistence and invocation overhead we don't need.

---

## 2026-04-21 — Single library, type-alias `Definition.t`

Phase 1 uses one library at `src/`, one executable at `bin/`, plus `test/`. `Definition.t` is a type alias for `Ast.t` rather than a singleton sum. The Store / Language boundary is visible at the module level (distinct `Store`, `Ast`, `Eval` modules inside the library) but not as separate libraries or sums.

**Rationale.** The split we first discussed (separate `core/` and `arith/` libraries, `Definition.t` as a singleton sum) was premature machinery at Phase 1's scale. When a second language arrives in a later prototype, refactoring the alias into a sum and splitting the library is a localized change. Keeping Phase 1 genuinely minimal is more valuable than pre-figuring the multi-language shape.

**Alternatives considered.** Separate `core/` and `arith/` libraries with a singleton-sum `Definition.t`. Aligns more tightly with the substrate's four-layer design but adds notation without measured benefit.

---

## 2026-04-21 — Big-step evaluator

Arithmetic evaluation is big-step: one recursive function from expression to value.

**Rationale.** Shorter and simpler than small-step for the prototype. Phase 1's thesis isn't about exposing reduction semantics in the UI. If we later want step-tracing, adding it is a localized change.

**Alternatives considered.** Small-step with a `:step <hash>` REPL command — more TAPL-faithful but costs complexity we haven't earned.

---

## 2026-04-21 — Bundled register + evaluate on bare expressions; explicit escape hatches

A bare expression at the REPL prompt is registered AND evaluated in one step, printing both the resulting hash and the value. `:register-only <expr>` registers without evaluating; `:eval-expr <expr>` evaluates without storing.

**Rationale.** Minimizes friction for the "just try things" feel that Phase 1 is about. The escape hatches keep register-without-eval and eval-without-caching reachable for anyone who needs them.

**Alternatives considered.** Require explicit `:register` and `:eval` commands for all operations. Rejected as too ceremonious for an interactive prompt.

---

## 2026-04-21 — Hand-rolled deterministic hash-input encoding

The function `Definition.t → bytes` used to feed BLAKE3 is hand-written: one tag byte per AST constructor, recurse into children. No yojson, no Marshal.

**Rationale.** Arithmetic's seven constructors make this ~20 lines. Hand-rolling is the most obviously correct option, adds no dependency, and survives OCaml version changes.

**Alternatives considered.** Marshal (not guaranteed deterministic across OCaml versions — risky for content-addressing). yojson (deterministic if careful, but adds a dependency for no benefit at this scale).
