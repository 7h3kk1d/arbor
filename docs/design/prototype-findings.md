# Prototype findings

**Status:** Stocktake. Observational, not prescriptive — nothing here is substrate policy unless `decisions.md` says it is.

## Purpose

A reflective view of what the prototype series has surfaced: patterns that have repeated across prototypes, experiments that one prototype has tried but no later prototype has retested, and shapes that have stayed open across the series.

The intended use is at boundaries between prototypes — read this before scoping a new one to see what's worth retesting, what's worth holding open, and what's still a single-data-point claim. Not a doc that drives the design forward; a doc that says where the design currently sits.

## What the series has produced

- **p1 (arithmetic).** Minimum register / lookup / evaluate loop. Baseline shape for the four-layer architecture.
- **p2 (structural sharing).** Shallow DAG-shaped storage; Attachment as a separate aspect store; first derived aspect (eval-cache).
- **p3 (naming layer).** First-class namespace; edit-time resolution; surface AST kept name-free at the type level; "no silent breakage" invariant.
- **p4 (λ-calculus).** First language with binders. De Bruijn internally with named surface input; α-equivalence via canonicalization; CBV-WHNF evaluation with a step budget.
- **p5 (multi-language).** First multi-language Store. Arith and λ-calculus side-by-side under `Definition.t = Arith | Lc`; first translator (`arith-to-lc-church`).
- **p6 (STLC).** First typed language. First non-Hash aspect value (`Type_of(Ty.t)`); first partial translator; first translator with inputs beyond the source.

## Patterns that have recurred

These have surfaced in two or more prototypes and held up. Promising candidates for substrate policy if a few more prototypes confirm the shape, but not yet committed.

- **Multi-language storage as a sum** (p5, p6). `Definition.t` as a sum over per-language node modules; per-language modules pattern-match. Adding a third language costs one variant plus per-language modules; Store / Attachment / Namespace shapes don't need to know.
- **One-byte language tag in node encoding** (p5, p6). Explicit byte-level separation of hash spaces ('A', 'L', 'S' so far). Cheap, visible (one grep tells you the language), no hash-space collisions.
- **"No cross-language references" enforced at two checkpoints** (p5, p6). At Store register time *and* at Resolver edit time. Same invariant, two natural surfaces — the user sees a clear edit-time error and the Store catches anything that slips past.
- **Translation as a derived aspect on the source** (p5, p6). `translation-to-<lang>:<translator-identity>` keys; cached value is a typed aspect-value variant. Multi-translator coexistence falls out for free.
- **Inline-at-resolution; no `Ref(hash)` yet** (p3, p4, p5, p6). The Resolver inlines a name's stored subtree at every use. Stored definitions are closed deep trees; closedness is what makes inlining at any binder depth safe in p4+. Re-encoding cost is real but tolerable at prototype scale.
- **Each prototype is a fresh tree, not edit-in-place** (p4, p5, p6). Preserves earlier prototypes' scope claims as historical artifacts; reinforces the disposable-prototype discipline.
- **Tests use deep-normalize helpers that aren't shipped as runtime modules** (p5, p6). Translator correctness is a β-normal-form property; runtime evaluators are CBV-WHNF. The test-only helper keeps prototypes small without adding speculative `Lc_deep_eval` infrastructure.

## Experiments tried once, not yet retested

These appeared in a single prototype. They look promising, but no later prototype has retested them. Treat as data points, not patterns.

### From p3
- **"Rename is derived" stance.** Renames change a name → hash binding without changing any program text, so they don't break callers. The Namespace records the change without re-resolving anything stored.
- **Dangling-display behavior for unnamed in-store subterms.** When the renderer encounters a hash with no namespace binding, it expands inline rather than printing the hash; only a missing hash prints a placeholder.

### From p4 (first language with binders)
- **Fresh display names from a fixed alphabet.** Deterministic per term, surface names not preserved through round trip. First language with binders, so first time the question arose.
- **Pretty-printer rule: Var children never collapse to namespace names.** Reasoning: a Var's meaning is context-dependent, a namespace name's is closed-term; collapsing them together hurts legibility.
- **Decline to cache when an input isn't in procedure identity.** `StepLimit` evaluator results are not cached — the step budget isn't part of `lc:eval:v1`, so caching would pin terms as divergent forever.

### From p6 (first typed language, first partial translator)
- **Refusal as a sibling aspect-value variant.** `Translation_untypable(string)` next to `Translation_target(Hash.t)`. Distinguishes "tried and refused" from "never tried"; replays of refused inputs short-circuit on the cached refusal.
- **Translator extra inputs folded into procedure identity.** `lc-to-stlc:check:v1[ty=<hex8>]`. Different inputs produce different cache cells; aspect-store schema unchanged. Cross-input listing requires prefix-matching procedure ids, covered by a small shim.
- **Inline structured aspect values beyond `Hash.t`.** `Type_of(Ty.t)` is inline rather than hashed. Justified by types being small in p6's STLC.
- **Per-language Store validity invariant.** "Every stored stlc definition type-checks" — enforced at Resolver, parallels p5's "no cross-language references" check at Store.
- **Separate hash-tag for non-definition values.** Types use a `'T'` tag in their encoding so type hashes can't collide with definition hashes when they appear inside procedure-identity suffixes.

These are listed without arguing for their promotion. The convention from `CLAUDE.md` is to migrate when learnings refine the substrate design, not eagerly; a second prototype using each shape would be a natural moment to revisit.

## Shapes that have stayed open

Some questions are deferred consistently across prototypes; others were raised once and parked. Both are informative — the persistent deferrals say which complexities have not yet earned their keep.

### Deferred consistently across the series
- **`Ref(hash)` as a first-class AST constructor.** Every prototype since p3 has inlined at resolution. Each acknowledges this defers most of "Phase 2" work. The cost (re-encoding on each name use) hasn't yet justified the complexity.
- **Garbage collection of orphan translations.** Translation entries point at old source hashes after a re-bind; correct, but dangling-looking. No prototype has hit a forcing case.
- **Mutual recursion canonicalization.** No language has needed it yet.
- **Reverse / lineage aspect entries on translation targets.** Always optional in `05-translation.md`; never built.
- **Migration discipline cashing out.** "Migrate when learnings refine the substrate design" is consistently invoked but rarely fires. Most prototype findings live in `docs/prototypes/<name>/` until at least a second prototype validates the shape.

### Raised once
- **η-equivalence in canonicalization** (p4). A per-language question; not forced.
- **Normal-form / deep evaluation as a runtime feature** (p4, p5). Tests use deep-normalize helpers; no `:normalize` REPL command.
- **Step-budget-aware caching beyond "don't cache StepLimit"** (p4).
- **Structured cause types for refused translations** (p6).
- **First-class extras field on the aspect-store key** (p6). Alternative to the procedure-identity suffix encoding.
- **Hashed type space for non-Hash aspect values** (p6). Alternative to inline `Type_of(Ty.t)`.
- **Holes / incomplete programs in the Store.** Raised in design docs and prototype open-questions; p8 on the prototype timeline is the place this gets tested.

## What might be worth watching for

The "second prototype validates the shape" convention cashes out as: the next prototypes worth weighting are ones that bring a second instance of one of the single-prototype experiments above.

- A second translator with inputs beyond the source would test the procedure-id-suffix shape against a structured extras field.
- A second partial translator would test `Translation_untypable`'s adequacy and let us see whether per-translator structured causes are wanted.
- A second typed language would test the "every stored definition is well-typed" invariant against a different type system.
- A prototype that introduces `Ref(hash)` would force a real test of the eager-translation closure behavior in `05-translation.md`.
- A prototype with holes in the Store (p8 on the timeline) would re-open the "every stlc definition type-checks" invariant — ill-typed / incomplete programs as first-class would need a different shape.

None of these is a roadmap commitment; the list is descriptive, not directive. Updating this doc after each prototype closeout would keep it honest as a stocktake.
