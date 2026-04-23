# Phase 6 — STLC with type-check aspect and bidirectional lc↔stlc translation

**Status:** Scope doc for the sixth prototype.
**Design context:** `../../design/02-definitions-and-derived-data.md` (asserted vs. derived aspects, procedure identity), `../../design/05-translation.md` (translators as derived aspects), `../../design/03-content-addressing.md` (no cross-language references), `../../design/06-architecture.md` (four-layer decomposition), `../../design/09-roadmap.md` §Beyond Phase 4 (STLC + type-check-as-aspect).
**Prototype design lives here:** `docs/prototypes/p6-stlc/`.
**Implementation code lives at:** `prototypes/p6-stlc/`.

## Thesis

`09-roadmap.md` §Beyond Phase 4 opens with:

> Type systems: STLC and its extensions; type-check as a derived aspect.
> Evaluation caching as a derived aspect; procedure identity in practice.

p6 is the first prototype that takes a type system as seriously as p1–p5 took evaluation and translation. It pairs the untyped λ-calculus (from p4/p5) with the **Simply-Typed λ-calculus of TAPL Ch. 8 + Ch. 9** — pure λ→ over a single `Bool` base with native `true`/`false`/`if`, every lambda annotated. Both languages live in one Store under `Definition.t = Lc | Stlc`.

Three new things appear in p6 that the substrate has not yet exercised:

1. **Type-check as a derived aspect.** `stlc:type-check:v1` stores the synthesized type of a stlc definition as `Type_of(Ty.t)`. First exercise of `02-definitions-and-derived-data.md`'s type-check example in a working prototype, and the first non-`Hash.t`-valued aspect in this series (eval and translation caches were both hash-valued).

2. **A partial translator.** `05-translation.md` has always allowed translators that "fail at runtime" on out-of-domain inputs, but p5's translator (`arith → lc`) was total. p6's `lc → stlc` translator is partial *by design* — most untyped lambda-terms are not simply-typable, and even those that are require a type hint. Refusals are recorded as `Translation_untypable(string)` in the aspect store, cached under the same key as a success would be, so replays short-circuit. This is the substrate's first worked answer to "how does a translator say *no*?"

3. **A translator with an input beyond the source.** `lc → stlc` takes an expected type from the user. The aspect-store's key is `(target, aspect, procedure)` — a fixed triple. p6 encodes the expected type into the procedure identity: `lc-to-stlc:check:v1[ty=<hex8>]`, where `<hex8>` is a short hash of `Ty.canonicalize(expected_ty)`. Different types → different procedure ids → distinct cache entries. First exercise of extending procedure identity to carry non-source arguments.

Arith is dropped. p5 already made the multi-language point with arith+lc; carrying a third language into p6 would dilute its focus without adding evidence. Disposable-prototype discipline (`CLAUDE.md` §Conventions).

### Questions this prototype should answer

1. Does the `Definition.t = Lc | Stlc` sum and the `'L'` / `'S'` tag-byte story reused from p5 carry cleanly when the second language is typed rather than merely different?
2. Is the **Store invariant "every stored stlc definition type-checks"** natural to enforce, or does it cause friction with the rest of the pipeline (ingest, reconstruction, pretty-printing, translation)? (Answer so far: cheap — type-checking runs in the Stlc resolver between surface→de-Bruijn and Store.ingest_stlc, and the rest of the stack assumes well-typedness without extra guards.)
3. Does `Type_of(Ty.t)` as an inline aspect value feel right, or would hashing types (a separate "ty" hash space) pay off? (p6 inlines; decision documented in `decisions.md` §Type_of is inline.)
4. How painful is constraint-based check-mode for untyped→typed translation without polymorphism? Does the monomorphic setting (no type variables reach the user) remove enough moving parts to make the translator readable? (Answer so far: the translator's core is ~100 lines of standard union-find HM-unification; the procedure-identity trick for caching is the interesting substrate-level addition.)
5. Does `Translation_untypable(string)` carry enough information, or do callers need more structured failure data? (Answer so far: enough for REPL display; structured reasons may matter if higher layers want to suggest type hints.)

## Languages

**Untyped lc** (`:lang lc`): p4/p5 verbatim.

```
t ::= x | \x. t | t t | (t)
```

**Simply-typed lc** (`:lang stlc`, TAPL Ch. 8+9 unioned):

```
t ::= x | \x:T. t | t t | (t)
    | true | false | if t then t else t

T ::= Bool | T -> T | (T)             (arrow right-associative)
```

Every lambda carries a type annotation in the surface form. Type annotations participate in hashing — `\x:Bool. x` and `\x:Bool -> Bool. x` produce distinct hashes.

## What "done" looks like

- `dune build` and `dune runtest` succeed.
- REPL session exercises all seven new commands:
  ```
  :lang stlc
  :bind not \x:Bool. if x then false else true
  :typecheck not                ; Bool -> Bool
  :typecheck not                ; Bool -> Bool (cached)
  :eval (not true)              ; false
  :translate not                ; -> lc hash (erase + Church-encode)
  :lang lc
  :bind i \x. x
  :translate i :: Bool -> Bool           ; -> stlc hash
  :translate i :: (Bool->Bool)->(Bool->Bool)  ; distinct cache entry
  :bind w \x. x x
  :translate w :: Bool -> Bool  ; Translation_untypable: cannot unify
  :translate w :: Bool -> Bool  ; (cached failure)
  :translations                 ; both successful and untypable, tagged by procedure
  ```
- Unit + qcheck tests cover: disjoint hash spaces, cross-language rejection, α-equivalence per language, Stlc well-typedness enforced at ingest, Stlc eval on native booleans, the type-check aspect cache, Stlc→Lc Church-erasure correctness via deep-β-normalization, Lc→Stlc success/failure cases, cache-per-expected-type independence, and an erase-preserves-booleans property.

## Out of scope (record in `open-questions.md`)

- `Ref(hash)` in either language — still inline at resolution, per p3/p4/p5.
- Polymorphism / System F / let-polymorphism / type variables — p6 is strictly monomorphic per TAPL Ch. 9.
- More base types (`Nat`, `Unit`) — Bool suffices to exercise the structure.
- Principal-type inference on lc — replaced by user-supplied target types.
- Automatic re-translation on source change, orphan-translation GC (deferred across all prototypes).
- A first-class aspect-store mechanism for "translator with extra inputs" — p6 uses procedure-id encoding; whether this graduates to an `extras` field on keys is a later question.
- `:translations` aggregated across every variant of `lc-to-stlc:check:v1[ty=*]` — today each type gets its own procedure id; aggregation is by prefix-match in the REPL.
- Type-erasure as a separate aspect (distinct from Church-encoding) — p6's stlc→lc translator does both in one pass; a pure-erasure variant that keeps booleans as (untyped) lambdas might be interesting later but would require either extending untyped lc with primitive booleans or accepting that the translator's output is Church-encoded.
