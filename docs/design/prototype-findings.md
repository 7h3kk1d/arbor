# Prototype findings

**Status:** Stocktake of p1–p6. Observational: nothing here is substrate policy unless `decisions.md` says so.

## Purpose

This doc records what the prototype series has surfaced: patterns that recurred across prototypes, experiments tried once and not retested, and questions that stayed open. Read it before scoping a new prototype to see what is worth retesting and which claims still rest on a single data point.

## What the series has produced

- **p1 (arithmetic).** Minimum register / lookup / evaluate loop; baseline shape for the four-layer architecture.
- **p2 (structural sharing).** Shallow DAG-shaped storage; Attachment as a separate aspect store; first derived aspect (eval-cache).
- **p3 (naming layer).** First-class namespace; edit-time resolution; surface AST kept name-free at the type level; "no silent breakage" invariant.
- **p4 (λ-calculus).** First language with binders. De Bruijn internally with named surface input; α-equivalence via canonicalization; CBV-WHNF evaluation with a step budget.
- **p5 (multi-language).** First multi-language Store: arithmetic and λ-calculus under `Definition.t = Arith | Lc`; first translator (`arith-to-lc-church`).
- **p6 (STLC).** First typed language. First non-Hash aspect value (`Type_of(Ty.t)`); first partial translator; first translator with inputs beyond the source.

## Patterns that have recurred

Each appeared in two or more prototypes and held up. They are candidates for substrate policy if further prototypes confirm them.

- **Multi-language storage as a sum** (p5, p6). `Definition.t` is a sum over per-language node modules, which pattern-match on it. A third language costs one variant plus its modules; Store, Attachment and Namespace need not know.
- **One-byte language tag in node encoding** (p5, p6). The tag separates hash spaces at the byte level ('A', 'L', 'S' so far). It is cheap, greppable, and rules out cross-language collisions.
- **"No cross-language references" enforced at two checkpoints** (p5, p6): at Store register time and at Resolver edit time. The user sees a clear edit-time error, and the Store catches anything that slips past.
- **Translation as a derived aspect on the source** (p5, p6). Keys have the form `translation-to-<lang>:<translator-identity>`, and the cached value is a typed aspect-value variant. Multiple translators coexist without extra machinery.
- **Inline-at-resolution; no `Ref(hash)` yet** (p3, p4, p5, p6). The Resolver inlines a name's stored subtree at every use. Stored definitions are closed deep trees, and closedness makes inlining safe at any binder depth from p4 on. Re-encoding cost is real but tolerable at prototype scale.
- **Each prototype is a fresh tree, not edit-in-place** (p4, p5, p6). Earlier prototypes' scope claims survive as historical artifacts.
- **Tests use deep-normalize helpers not shipped as runtime modules** (p5, p6). Translator correctness is a β-normal-form property, while runtime evaluators are CBV-WHNF. A test-only helper avoids adding a speculative `Lc_deep_eval` module.

## Experiments tried once, not yet retested

Each appeared in a single prototype. They are data points, not patterns. Per `CLAUDE.md`, learnings migrate when they refine the substrate design; a second prototype using a shape is the natural moment to revisit it.

### From p3
- **Rename is derived.** A rename changes a name → hash binding without touching program text, so callers do not break. The Namespace records the change without re-resolving anything stored.
- **Dangling display for unnamed in-store subterms.** A hash with no namespace binding is expanded inline; only a hash missing from the store prints a placeholder.

### From p4 (first language with binders)
- **Fresh display names from a fixed alphabet.** Deterministic per term; surface names are not preserved through a round trip.
- **Var children never collapse to namespace names in the pretty-printer.** A Var's meaning depends on context and a namespace name denotes a closed term, so merging them hurts legibility.
- **No caching when an input is outside procedure identity.** `StepLimit` results are not cached: the step budget is not part of `lc:eval:v1`, so caching would pin terms as divergent forever.

### From p6 (first typed language, first partial translator)
- **Refusal as a sibling aspect-value variant.** `Translation_untypable(string)` sits next to `Translation_target(Hash.t)`, distinguishing "tried and refused" from "never tried". Replays of refused inputs short-circuit on the cached refusal.
- **Translator extra inputs folded into procedure identity.** `lc-to-stlc:check:v1[ty=<hex8>]`. Different inputs get different cache cells, and the aspect-store schema is unchanged. Listing across inputs needs prefix-matching on procedure ids, handled by a small shim.
- **Inline structured aspect values beyond `Hash.t`.** `Type_of(Ty.t)` is stored inline rather than hashed, justified by types being small in p6's STLC.
- **Per-language Store validity invariant.** "Every stored stlc definition type-checks", enforced at the Resolver, parallel to p5's "no cross-language references" check at the Store.
- **Separate hash tag for non-definition values.** Type encodings use a `'T'` tag so type hashes cannot collide with definition hashes inside procedure-identity suffixes.

## Shapes that have stayed open

Some questions were deferred in every prototype; others were raised once and parked. The persistent deferrals show which complexities have not yet earned their cost.

### Deferred consistently across the series
- **`Ref(hash)` as a first-class AST constructor.** Every prototype since p3 has inlined at resolution, which defers most of the roadmap's Phase 2 work. Re-encoding on each name use has not yet justified the complexity.
- **Garbage collection of orphan translations.** After a rebind, translation entries still point at old source hashes. This is correct but looks dangling. No prototype has hit a forcing case.
- **Mutual recursion canonicalization.** No language has needed it yet.
- **Reverse / lineage aspect entries on translation targets.** Optional in `05-translation.md`; never built.
- **Migration discipline.** "Migrate when learnings refine the substrate design" is often invoked but rarely fires. Most findings stay in `docs/prototypes/<name>/` until a second prototype validates them.

### Raised once
- **η-equivalence in canonicalization** (p4). Per-language; not forced.
- **Normal-form / deep evaluation as a runtime feature** (p4, p5). Tests use deep-normalize helpers; there is no `:normalize` REPL command.
- **Step-budget-aware caching beyond "don't cache StepLimit"** (p4).
- **Structured cause types for refused translations** (p6).
- **First-class extras field on the aspect-store key** (p6), as an alternative to the procedure-identity suffix encoding.
- **Hashed type space for non-Hash aspect values** (p6), as an alternative to inline `Type_of(Ty.t)`.
- **Holes / incomplete programs in the Store.** Raised in design docs and prototype open questions; p8 is where it gets tested.

## What might be worth watching for

The prototypes worth weighting next are those that bring a second instance of a single-prototype experiment:

- A second translator with inputs beyond the source would test the procedure-id suffix against a structured extras field.
- A second partial translator would test whether `Translation_untypable` suffices or per-translator structured causes are wanted.
- A second typed language would test the "every stored definition is well-typed" invariant against a different type system.
- A prototype that introduces `Ref(hash)` would test the eager-translation closure in `05-translation.md`.
- A prototype with holes in the Store (p8) would reopen the "every stlc definition type-checks" invariant, since first-class ill-typed or incomplete programs need a different shape.

None of these is a roadmap commitment. Updating this doc at each prototype closeout keeps the stocktake current.
