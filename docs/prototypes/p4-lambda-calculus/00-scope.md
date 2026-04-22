# Phase 4 — Untyped λ-calculus with de Bruijn indices

**Status:** Scope doc for the fourth prototype.
**Design context:** `../../design/03-content-addressing.md` (canonicalization, α-equivalence), `../../design/04-naming-layer.md` (namespace, edit-time resolution), `../../design/06-architecture.md` (four-layer decomposition), `../../design/09-roadmap.md:84-107` (Phase 3 — "Untyped λ-calculus with naming and de Bruijn indices").
**Prototype design lives here:** `docs/prototypes/p4-lambda-calculus/`.
**Implementation code lives at:** `prototypes/p4-lambda-calculus/`.

## Thesis

p3-naming-layer validated that a namespace can coexist with content-addressed storage for a simple first-order language (untyped arithmetic). p4-lambda-calculus carries the same naming-layer machinery forward to a language with **binders**: the untyped λ-calculus from TAPL Ch. 5.

- The **internal `Ast.t`** is in de Bruijn form. Binders carry no names; variables are integer indices, 0 = innermost enclosing binder. `λx. x` and `λy. y` produce *identical* internal ASTs and therefore identical content hashes after `Store.ingest`. α-equivalence is resolved at hashing time — the substrate's first concrete demonstration of the canonicalization-via-de-Bruijn claim from `03-content-addressing.md`.
- The **surface AST** keeps string names (`Var(string) | Lam(string, t) | App(t, t)`) as a parsing/lookup hint. Names never persist past the Resolver.
- The **Resolver** threads a context of enclosing binder names while walking surface input. Variable lookups check the context first; innermost binder wins. Unshadowed names fall through to the definition-level Namespace and resolve the standard p3 way — look up, reconstruct, inline. Stored definitions are closed terms (no free variables), so inlining needs no de Bruijn shifting regardless of binding depth at the use site.
- The **name-aware pretty-printer** walks a stored hash and assigns fresh display names to each λ-binder it crosses. Child subterms with a namespace binding collapse to the name (p3 behavior); the top-level hash is never collapsed; bound-variable positions never collapse to namespace names even if a coincidental namespace binding points at the corresponding Var hash.
- The **evaluator** is a big-step call-by-value β-reducer in weak head normal form, memoized through the Attachment layer. A step budget bounds non-termination: evaluating the Omega combinator `(λx. x x) (λx. x x)` returns `StepLimit`, not a stack overflow. `StepLimit` results are never cached because they are partial.

The prototype continues p3's "inline-at-resolution" stance — no `Ref(hash)` AST constructor. Cross-definition references still bottom out to inlined closed subtrees. Validating `Ref(hash)` remains roadmap Phase 2's unfinished scope, deferred to a later prototype.

### Questions this prototype should answer

1. Does de Bruijn canonicalization deliver α-equivalent hashing in practice? Does `\x. x` / `\y. y`, `\x. \y. x` / `\a. \b. a`, and subtler cases all collide as expected?
2. Does the context-threaded resolver feel right? Is the difference between definition-level names (Namespace → hash) and bound variables (context → de Bruijn index) legible when using the REPL? Does shadowing match a reader's intuition without special-casing?
3. Does the fresh-names-per-render strategy read well? Do the names the user types survive round-trip or do they get renamed on every display? What is the renderer's stability contract?
4. Does β-reduction under a step limit handle the untyped-LC reality of non-termination gracefully? Is the `StepLimit` distinction useful?
5. What's missing once you've used it that wasn't missing in p3? Candidates: `Ref(hash)` for reuse efficiency, normal-order reduction, interactive step traces, a primitive parenthesized-sequence for Church-style encodings, named holes.

## Language

Untyped λ-calculus (TAPL Ch. 5). Surface syntax:

```
t ::= x                 -- variable (bound or definition name)
    | \x. t             -- lambda abstraction (ASCII backslash for λ)
    | t t               -- application, left-associative
    | (t)               -- grouping
```

- Binder uses ASCII `\` as a stand-in for λ; `.` separates binder from body.
- Application is juxtaposition, left-associative: `f x y` parses as `(f x) y`.
- Lambda bodies extend as far right as possible: `\x. f x` is `\x. (f x)`, not `(\x. f) x`.
- Identifier lexing: `[a-zA-Z_][a-zA-Z0-9_]*`.
- No reserved keywords at the surface level. Namespace's reserved-name list is empty; the hook remains for REPL-owned words if one ever appears.

## Architecture mapping

Four layers, shape parallel to p3:

- **Store.** Unchanged shape. `Node.t` has three constructors (`Var(int)`, `Lam(Hash.t)`, `App(Hash.t, Hash.t)`). Tag bytes `\x01` / `\x02` / `\x03`; variable indices encoded big-endian as 8 bytes.
- **Attachment.** Unchanged shape. Eval cache is still the only registered descriptor: `"lc:eval"` with procedure `"lc:eval:v1"`, disposition `Derived`. The `aspect_value` variant gains a third case, `Eval_step_limit(Hash.t)`, for the bounded-timeout result.
- **Language.** New `Ast` (de Bruijn, with `shift` / `subst` / `beta` helpers), `Surface_ast` (named, for parser/renderer), `Node` (shallow storage), `Parser` / `Lexer`, `Eval`, `Canonicalize` (identity), `Resolver` (context-threaded), `Pretty` (fresh-name generator).
- **Interface.** REPL mirrors p3's command set. New commands: `:step-limit [n]` to query or set the current β-reduction bound; `:list closed` to enumerate only the stored rows whose hash reconstructs to a closed term. New CLI flag: `--step-limit N`.

## In scope

- Grammar and lexer for `\x. t`, left-associative application, variables, parentheses.
- `Surface_ast.t` with `Var(string) | Lam(string, t) | App(t, t)`.
- `Ast.t` in de Bruijn form with `shift`, `subst`, `beta`, `is_closed`, `max_free_index`.
- `Node.t` with three shallow constructors and its own encoding/hashing.
- Updated `Store.ingest` / `Store.reconstruct` for the new Node shape.
- Context-threaded `Resolver.resolve` that looks up bound variables first, falls through to Namespace, inlines closed subtrees; errors are `Unbound_name` or `Missing_hash`.
- Namespace carried from p3 with reserved-keyword list drained.
- `Canonicalize.canonicalize` as identity.
- `Eval.eval` with a `~step_limit` parameter (default 10000). Result is `Value | Stuck | StepLimit`. Memoized through Attachment; the `StepLimit` case is excluded from cache writes (so the next call with a higher limit can succeed).
- `Pretty.surface_of_hash` / `print_named` with a deterministic fresh-name generator (alphabet `x, y, z, a, …, w` with numeric suffix on clash) and p3-style namespace collapsing for non-binder children. Top-level hash never collapsed.
- REPL with p3 commands plus `:step-limit`. `--load`, `--no-repl`, `--step-limit` CLI flags.
- Bootstrap script at `scripts/bootstrap.repl` with `id`, `const`, `flip`, `apply`, `comp`, Church booleans, Church numerals, the Y combinator, and `omega`.
- Tests: parser cases, shift/subst/beta unit tests, ingest/reconstruct roundtrip (qcheck), ingest determinism (qcheck), closedness of generated terms (qcheck), structural sharing, α-equivalence collision cases (4 specific pairs), per-redex β-reduction, step-limit on Omega, step-limit non-caching, cache hit on re-eval, resolver shadowing / fallthrough / unbound, nested-binder resolution, surface round-trip (render → re-parse → same hash), Namespace ops carried from p3, no-silent-breakage, pretty-printer rules, hash display.

## Out of scope

- `Ref(hash)` as a first-class AST constructor (roadmap Phase 2's remaining scope; still deferred).
- Types. STLC is a later prototype.
- Multiple languages in one Store or translation between them (roadmap Phase 4).
- Named holes, hole-aware evaluation, collaborative editing.
- Multiple namespaces, branching, persistence.
- Hierarchical name conventions, language qualifiers.
- Normal-order reduction and full normalization beyond WHNF.
- Arithmetic. This prototype is λ-calculus-only per the roadmap.

## What "done" looks like

- `dune build && dune runtest` is green inside `prototypes/p4-lambda-calculus/`. Current suite: 43 tests (parser, shift/subst, ingest qcheck + sharing, α-equivalence, eval, resolver, namespace, no-silent-breakage, pretty, hash display).
- REPL session:
  - `:bind id \x. x` and `:bind id_y \y. y` both produce the same short hash. `:hash-of id` and `:hash-of id_y` print identical full hashes.
  - `:bind const \x. \y. x`, `:bind konst \a. \b. a`, `:bind tru \t. \f. t` all point at a single hash — α-equivalence collapses all three.
  - `const id id` evaluates to `\x. x` and renders as such.
  - `(\x. x x) (\x. x x)` reports `… step limit reached at: (\x. x x) (\x. x x)` (or similar) under the default 10000-step budget, and does not cache the result.
  - `:list` shows definition-level names on the left (`[id, id_y]`, `[const, konst, tru]`) and renders stored bodies with freshly generated binder names; named child subterms collapse to their names.
- Bootstrap script at `scripts/bootstrap.repl` loads cleanly (`--load scripts/bootstrap.repl --no-repl` exits without error).

## Directory layout

```
prototypes/p4-lambda-calculus/
  dune-project
  p4_lambda_calculus.opam       # generated by dune
  _opam/                        # local opam switch (initially symlinked to p3's)
  src/
    dune
    ast.re                      # de Bruijn AST + shift / subst / beta
    surface_ast.re              # string-carrying AST for parser/renderer
    node.re                     # shallow storage node; tag bytes + encoding
    parser.mly                  # \x. t | t t | (t) | ident
    lexer.mll                   # BACKSLASH, DOT, LPAREN, RPAREN, IDENT
    canonicalize.re             # identity
    hash.re                     # unchanged from p3
    store.re                    # Node-shaped ingest / reconstruct
    attachment.re               # + Eval_step_limit variant
    definition.re               # alias to Node.t
    eval.re                     # CBV β-reduction, step budget, cached
    namespace.re                # p3's module with reserved list drained
    resolver.re                 # context-threaded; inlining preserved
    pretty.re                   # raw + name-aware with fresh-name generator
  bin/
    dune
    main.re                     # REPL; p3 commands + :step-limit
  test/
    dune
    test_p4.re                  # alcotest + qcheck suite
  scripts/
    bootstrap.repl              # id, const, flip, apply, Church numerals, Y
```

## Tech stack

Same as p3: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.16, Menhir, ppx_deriving, digestif (BLAKE2B), alcotest, qcheck. The prototype initially shares p3's opam switch via a symlink at `prototypes/p4-lambda-calculus/_opam/`; once the prototype stabilizes, a dedicated switch should be materialized here, matching p3's convention (see `../p3-naming-layer/decisions.md`, entry dated 2026-04-22).
