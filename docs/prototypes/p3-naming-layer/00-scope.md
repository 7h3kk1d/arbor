# Phase 3 — Naming Layer

**Status:** Scope doc for the third prototype.
**Design context:** `../../design/04-naming-layer.md` (namespaces), `../../design/03-content-addressing.md` ("no silent breakage"), `../../design/06-architecture.md` (Store + Attachment + Interface).
**Prototype design lives here:** `docs/prototypes/p3-naming-layer/`.
**Implementation code lives at:** `prototypes/p3-naming-layer/`.

## Thesis

p2-structural-sharing validated shallow, DAG-shaped storage plus the Attachment aspect store with an eval cache. p3-naming-layer layers a **first-class namespace** of name → hash bindings on top of that same substrate shape.

- A `Namespace` module (not an aspect) holds `name ↔ hash` bindings with bidirectional queries.
- Names in source expressions are resolved **at edit time**, before ingest. Stored programs carry no names — the internal `Ast.t` is name-free by construction; a separate `Surface_ast.t` carries `Name(string)` leaves between parser and resolver.
- A **name-aware pretty-printer** rewrites stored DAGs into surface ASTs where any child subterm with a bound name collapses to the name. This makes structural sharing legible to humans, not just to hashes.
- Rebinding a name **never mutates stored programs**. This is the "no silent breakage" property from `03-content-addressing.md`, now visible in a real interface.

The prototype intentionally stops short of roadmap Phase 2's `Ref(hash)` first-class AST constructor: names are resolved to inlined subtrees before ingest. This keeps p2's DAG and hashing model identical while still validating everything the naming layer is asked to do at substrate level.

### Questions this prototype should answer

1. Does `Namespace` as a peer module of `Attachment` (both hang off the Store but with different keying and invariants) feel right in practice? Is the "don't unify them until branching" stance from `04-naming-layer.md:56-65` paying for itself?
2. Does edit-time resolution via a separate `Surface_ast.t` produce legible code and meaningful structural sharing? Is the type-level split between internal and surface ASTs worth the extra module?
3. Is the "no silent breakage" property legible when a user rebinds a name? Does it surprise them in a good way or a bad way?
4. Does the name-aware `:list` output actually improve readability, or does it obscure the DAG shape?
5. What's missing once you've used it — `Ref(hash)` in the AST? Hierarchical names? Renaming as an atomic op?

## Language

Identical to p1 and p2: TAPL Ch. 3 untyped arithmetic, with the one surface-syntax addition that identifier tokens now parse as `Surface_ast.Name(_)` atoms. The lexer matches keywords before identifiers, so `succ`, `pred`, `iszero`, `true`, `false`, `if`, `then`, `else`, and the literal `0` are untouchable.

## Architecture mapping

Four of the four substrate layers participate:

- **Store.** Unchanged from p2. Still shallow `Node.t` keyed by `Hash.t`; `ingest` still takes a name-free `Ast.t`.
- **Attachment.** Unchanged. Eval cache aspect is the only descriptor registered.
- **Language.** Arith module gains `Surface_ast`, `Namespace`, and `Resolver`. Parser produces `Surface_ast.t`; Resolver converts it to `Ast.t` by substituting names with reconstructed subtrees.
- **Interface.** REPL gains `:bind`, `:rebind`, `:bind-hash`, `:unbind`, `:rename`, `:names`, `:name-of`, `:hash-of`, a name-aware `:list`, and `:list raw` as an escape hatch.

## In scope

- `Namespace.t` with `bind`, `rebind`, `unbind`, `rename`, `resolve`, `names_of`, `entries`, `size`. Opaque, parameterized, bidirectional.
- A separate `Surface_ast.t` structurally parallel to `Ast.t` with an added `Name(string)` constructor. A one-way `Surface_ast.of_ast` lifter for the raw printer.
- Parser produces `Surface_ast.t`; identifier tokens match `[a-zA-Z_][a-zA-Z0-9_]*` after the keyword rules.
- `Resolver.resolve(~namespace, ~store, Surface_ast.t)` → `result(Ast.t, error)`. `Name(s)` substitution uses `Namespace.resolve` followed by `Store.reconstruct` to inline the bound subtree.
- Name-aware pretty-printer: `Pretty.surface_of_hash(~namespace, store, h)` produces a `Surface_ast.t` where child subterms with bound names collapse to `Name(first-alphabetical-name)`; the top-level hash itself is never substituted.
- REPL `:list` row format: `<short-hash> [name, alias] — <body with named children substituted>`. Brackets omitted when the hash has no names.
- Reserved-keyword collision check at bind time; same list used by lexer and namespace.
- Tests: all p2 tests carried forward; new Namespace unit tests; Resolver tests (substitution, unbound error, edit-time structural sharing); the "no silent breakage" invariant test; name-aware pretty-printer tests (substitution, top-level not collapsed, alphabetical tiebreak, empty-namespace matches raw).

## Out of scope

- `Ref(hash)` as a first-class AST constructor. Naming is validated here; `Ref(hash)` is deferred to a later prototype (roadmap Phase 2's remaining scope).
- Multiple namespaces. The `Namespace.t` API already takes a namespace parameter — multi-namespace is additive, but no second namespace is created.
- Namespace branching, merging, diffing, history, time-travel. Still deferred to the eventual branching phase.
- Persistence. Process-local Store, Attachment, and Namespace.
- Hierarchical name conventions (dot-paths, slash-paths, language qualifiers). Names stay opaque.
- Suffix-based disambiguation (Unison's `f.h1a2b3`). Our namespace is unambiguous by construction.
- Automated "update all callers" on rebind. Upgrading callers is manual by design.
- Procedure content-addressing. Tag+version strings still, same as p2.

## Bootstrap script

A seed corpus lives at `scripts/bootstrap.repl`. It is a text file of REPL commands with `#`-prefixed comments and blank-line tolerance. Two ways to use it:

- At startup: `dune exec bin/main.exe -- --load scripts/bootstrap.repl`.
- Mid-session: `:load scripts/bootstrap.repl`.

Multiple `--load FILE` flags stack in order. `--no-repl` runs all loads and exits (useful for checking the script builds cleanly). Each executed line is echoed with a `▸ ` prefix so the transcript reads clearly.

## What "done" looks like

- `dune build && dune runtest` is green. 36 tests run: 21 carried from p2, 15 new.
- In the REPL:
  - `:bind one succ 0` produces `bound one -> h:...`.
  - `:bind two succ one` succeeds (name resolution inside the expression).
  - `:list` shows rows like `#abc123 [one]   — succ 0` and `#def456 [two]   — succ one` — substructure labeled.
  - `:rebind one pred (succ 0)` updates the binding but `:list` still shows `two`'s body as containing the old `succ 0` (now unnamed), because the stored `Node.Succ` child hash hasn't changed.
  - `:name-of #abc123` lists names pointing at a hash; `:hash-of one` prints the full hash. `:bind <name> #abc123` binds by hash prefix; `:bind <name> <expr>` binds by an expression (with edit-time name resolution).
- The `no-silent-breakage` test pins `Store.lookup(h_two)`'s `Node.Succ` child hash unchanged across `:rebind one`.

## Directory layout

```
prototypes/p3-naming-layer/
  dune-project
  p3_naming_layer.opam       # generated by dune
  src/
    dune
    ast.re             # internal, name-free AST (consumed by Store)
    surface_ast.re     # NEW — parser/printer AST with Name(string)
    node.re            # unchanged from p2
    parser.mly         # + IDENT token; start symbol now Surface_ast.t
    lexer.mll          # + identifier rule after keywords
    canonicalize.re    # unchanged (identity)
    hash.re            # unchanged
    store.re           # unchanged; still takes Ast.t
    attachment.re      # unchanged
    definition.re      # unchanged
    eval.re            # unchanged
    namespace.re       # NEW — bidirectional name ↔ hash
    resolver.re        # NEW — Surface_ast.t → result(Ast.t, error)
    pretty.re          # extended: surface_of_hash + print_surface + print_named
  bin/
    dune
    main.re            # REPL with namespace commands + name-aware :list
  test/
    dune
    test_p3.re         # alcotest + qcheck suite
```

## Tech stack

Same as p1 and p2: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.16, Menhir, ppx_deriving, digestif (BLAKE2B), alcotest, qcheck. The prototype has its own local opam switch at `prototypes/p3-naming-layer/_opam/` with OCaml 5.2.0, matching p2's switch setup.
