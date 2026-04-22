# Phase 2 — Structural Sharing + Eval-Cache Aspect

**Status:** Scope doc for the second prototype.
**Design context:** `../../design/06-architecture.md` (Store + Attachment), `../../design/02-definitions-and-derived-data.md` (aspects), `../../design/03-content-addressing.md` (hash semantics).
**Prototype design lives here:** `docs/prototypes/p2-structural-sharing/`.
**Implementation code lives at:** `prototypes/p2-structural-sharing/`.

## Thesis

p1-arithmetic validated the minimum register / lookup / evaluate loop with "deep" storage — the Store held one entry per top-level term, hashed over the full subtree inline. p2 moves content addressing *inside* the tree.

- Every subexpression is hashed and lives in the Store as a **shallow node**: one constructor + hashes of its children. The Store becomes a DAG.
- Two expressions that share a subterm share storage for it automatically, by virtue of identical hashes.
- Evaluation results are memoized via the **Attachment layer** as a derived aspect (`arith:eval`, procedure identity `arith:eval:v1`). Repeated subexpressions hit the cache, within and across top-level registrations.

This prototype sits off the roadmap's main phase grid. The roadmap's Phase 2 is "naming + cross-definition references" and Phase 4 is where aspects first appear (for translation caching). p2-structural-sharing pulls the aspect machinery forward for eval caching in a single-language setting, and skips the naming layer. Explicitly allowed by CLAUDE.md's disposable-prototype posture.

### Questions this prototype should answer

1. Does shallow, DAG-shaped storage work in code, and does it produce meaningful structural sharing in practice?
2. Does the substrate's Attachment-layer API shape (aspect-id + procedure-identity + asserted/derived + bidirectional queries) feel right for a real, simple aspect?
3. Is content-addressed evaluation memoization observable and useful, and what's its shape at the API?
4. Does a pretty-printer that has to walk the Store to reconstruct trees compose cleanly with content-addressed storage?

## Language

Identical to p1: TAPL Ch. 3 untyped arithmetic. The surface syntax, parser, and lexer carry over unchanged. What changes is how the parsed tree is *stored* and *evaluated*.

## Architecture mapping

Three of the four substrate layers participate (Interface plus the two new ones):

- **Store.** Shallow `Node.t` keyed by `Hash.t`. New primitives: `ingest` (walks a deep `Ast.t` bottom-up, hash-consing every subterm) and `reconstruct` (walks the DAG back to an `Ast.t` for display).
- **Attachment.** The aspect store. Exercises the full API described in `06-architecture.md:34-42`: descriptors (id + disposition + languages), `(target, aspect, procedure) → value` entries, bidirectional queries. Only one descriptor is registered — the eval cache — but the API shape is the real one.
- **Language.** arith module: parser/lexer (from p1), canonicalizer (still identity), shallow-node hashing, and a memoized big-step evaluator over hashes.
- **Interface.** REPL, extended with `:stats` and an inline `(cached)` marker on top-level cache hits.

## In scope

- Shallow `Node.t` stored in the Store. Every child is a `Hash.t`, never inline.
- `ingest : Ast.t → Hash.t` — bottom-up hash-consing. Repeated subterms register once.
- `reconstruct : Hash.t → option(Ast.t)` — DAG walk back into a deep AST.
- Evaluator returns `Hash.t`. Values produced during evaluation (e.g., `succ 0` emerging from `pred (succ (succ 0))`) are registered in the Store and returned by hash.
- Full Attachment layer, minimally populated with the eval-cache aspect. Procedure identity as a tag+version string (`arith:eval:v1`), per `02-definitions-and-derived-data.md`'s near-term path.
- Stuck terms are cached (stuckness is a deterministic function of (term, evaluator version)).
- REPL visibility: inline `(cached)` on top-level hits; `:stats` reports definition count, eval-entry count, hits, misses.
- Tests: parser cases (from p1); ingest/reconstruct roundtrip (qcheck); shallow-hash determinism (qcheck); structural sharing (`succ 0` registered once across multiple host expressions); per-constructor eval; stuck-term caching; repeated-eval cache hit; cross-expression cache hit; Attachment bidirectional query; derived-immutability semantics.

## Out of scope

- Naming layer — all references are still by hash. (The roadmap's Phase 2 territory.)
- Multiple languages, translation. (Phase 4.)
- Cross-definition `Ref(hash)` as a first-class AST construct between separately-registered TOP-LEVEL terms. Structural sharing here is purely a storage/hash property; the language's AST has no `Ref` constructor yet.
- Persistence. Process-local Store + Attachment, same as p1.
- Asserted aspects. Only the derived path is exercised; asserted is declared in the types but unused.
- Procedure content-addressing. Tag+version strings only.

## What "done" looks like

- You enter `pred (succ (succ 0))` in the REPL; it registers the full DAG (four shallow nodes) and evaluates to `succ 0` (whose hash has been attached to the cache).
- You enter `succ 0`; the REPL reports the *same hash that was stored as the intermediate value* and prints `(cached)` — content-addressed structural sharing is visible end-to-end.
- `:stats` shows definition count, cache entries, and hit/miss counters.
- `:list` enumerates the DAG; you can `:lookup` and `:eval` by hash prefix just like p1.
- Tests pass: ingest/reconstruct roundtrips, hash determinism, structural sharing, repeated-eval cache hit, cross-expression cache hit, bidirectional query, stuck-term caching.
- You have honest opinions about whether the Attachment API shape fits a real aspect, and the next most interesting question is clear.

## Directory layout

```
prototypes/p2-structural-sharing/
  dune-project
  p2_structural_sharing.opam       # generated by dune
  src/
    dune
    ast.re             # DEEP AST — parser's output, pre-ingest
    node.re            # SHALLOW node — what lives in the Store
    parser.mly         # identical to p1
    lexer.mll          # identical to p1
    canonicalize.re    # identity for arith
    hash.re            # hash over (tag ++ child hashes); BLAKE2B
    store.re           # Hashtbl(Hash.t, Node.t) + ingest + reconstruct
    attachment.re      # NEW: aspect store with bidirectional queries
    definition.re      # type t = Node.t — consistent with p1's pattern
    pretty.re          # walks Store to reconstruct + print
    eval.re            # memoized big-step evaluator over hashes
  bin/
    dune
    main.re            # REPL with :stats + (cached) markers
  test/
    dune
    test_p2.re         # alcotest + qcheck suite
```

## Tech stack

Same as p1: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.16, Menhir, ppx_deriving, digestif (BLAKE2B), alcotest, qcheck. Fresh local opam switch inside the prototype directory.
