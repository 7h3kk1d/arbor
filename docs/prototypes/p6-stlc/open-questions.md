# Open Questions — p6-stlc

Prototype-specific open items. Distinguishes "positions taken here (see `decisions.md`)" from "intentionally deferred."

## Positions this prototype takes

- **Languages: `Lc | Stlc`** — arith dropped, because p5 already made the multi-language point.
- **Types: `Bool | Arrow`** — TAPL Ch. 9 literal, no type variables, no polymorphism.
- **STLC is TAPL Ch. 8+9 unioned** — native `true`/`false`/`if` alongside λ→.
- **Store invariant: every stored stlc definition type-checks** — enforced at Resolver time, not retroactively.
- **Aspect-value additions** — `Type_of(Ty.t)` (inline, not hashed) and `Translation_untypable(string)`.
- **Lc→Stlc cache keying via procedure-id suffix** — `lc-to-stlc:check:v1[ty=<hex8>]`.
- **Lc→Stlc algorithm** — constraint-based HM-style unification with a zonk pass; no type variables surface to the user.
- **REPL `::` syntax** for target type on lc→stlc.

## Still open — intentionally deferred

### Type aspect

- **Conflict between asserted and derived type entries.** `docs/design/02-definitions-and-derived-data.md` §Open sub-questions imagines a user annotating `foo : Bool -> Bool` and the derived type-check disagreeing. p6 has no asserted type-annotation-on-definition aspect (annotations live *inside* STLC terms, on binders). If a later prototype adds `foo : Bool -> Bool` as metadata at the Namespace or Attachment layer, the conflict-resolution story reappears.
- **Hashing Ty.** p6 inlines `Ty.t` in `Type_of`. A later prototype with many types, or with a structured type-term language large enough that duplication hurts, might benefit from a separate `Ty.hash` space and `Type_of(Hash.t)` values. Not urgent.
- **Richer failure for type-check.** Today ill-typed stlc is rejected at Resolver time, so the aspect cache only ever holds successes. If a future prototype relaxes this (Hazel-style "type-error as a first-class aspect value"), we'd add `Type_error(string)` or similar.

### Translation

- **Aggregated listing of `lc-to-stlc:check:v1[ty=*]`.** Today, `Attachment.entries_for(procedure=…)` gives one procedure's entries; listing all variants requires a prefix match (see `Lc_to_stlc_check.all_entries`, which folds the store manually). Adding a first-class "prefix query" to Attachment is a small change; defer until we have a second translator-with-inputs.
- **Generalized "translator with extra inputs" key shape.** `Attachment.key` could gain an optional `extras` field carrying ground type hashes or other disambiguators. p6's procedure-identity-suffix trick is fine for one translator; revisit when two or more want it.
- **Reverse translator (stlc → lc via pure erase, no Church encoding).** Today stlc→lc both erases *and* Church-encodes booleans. A "pure erase" variant would output something that's not Church-encoded (and therefore can't evaluate native booleans), which means extending untyped lc with primitives. Arguably that's a new language; not p6's job.
- **Automatic re-translation on source change; orphan-translation GC.** As in p5 and earlier: deferred across the series.
- **Certifying Lc→Stlc correctness by property.** "Type of Lc→Stlc(u, T) equals T" is a one-line test; we test it. A more thorough property (the translated stlc term's erasure equals the original untyped term up to α-equivalence) is also straightforward; we don't currently test it but should if the translator grows.
- **Partial-translator representation** with `Translation_untypable(string)` is new in p6. If it survives a second partial translator (any prototype after p6 that needs one), migrate the convention to `docs/design/05-translation.md` as a worked example of refusal-caching.

### Transitive dependencies

- **`Ref(hash)` in either language.** Still deferred; p3/p4/p5 all inline at resolution, and p6 follows. When it arrives, the Lc→Stlc translator will need to walk deps — but since types are supplied at the top level, each dep would need an inferred type, and the translator would have to either (a) recur with the derived domain/codomain types, or (b) require the user to supply types for each named dep. Noted for when that prototype happens.

### Evaluators and normalization

- **Stlc deep β-normalization.** Test code uses p5's `deep_normalize` helper for the *lc side* of stlc→lc translator correctness. Native stlc eval is still CBV-WHNF and doesn't reduce under binders. If a future REPL wants `:normalize <stlc-term>`, Stlc_eval would need a deep-mode equivalent. Not urgent.

### Pretty-printing

- **Reverse-aspect display for translation targets.** A stlc hash that was produced by `lc-to-stlc:check:v1[ty=…]` looks identical in `:list` to a stlc hash the user typed. A "translated from lc:<hash> at <type>" annotation (via an optional lineage aspect on the target) would help. Deferred with the similar p5 open question.
- **Type rendering in `:show`.** Stlc Lam rows show `Lam :T <hash>` with `T` via `Ty.print`. Longer types may want line-wrapping; not urgent.

### REPL UX

- **`:translate <name|#pfx> :: <type>` lookup ergonomics.** The `::` ascription is parsed from the raw argument string; if the user omits a space before `::`, parsing still works. If the user's hash-prefix contains `::`... it can't, hexadecimal doesn't. Fine.
- **`:typecheck-expr <e>`.** No REPL affordance for "type-check this bare expression without binding." Would mirror `:eval-expr`. Mechanical to add.
- **Listing by language for `:typecheck-all`.** `:types` shows every stored type; there's no per-language filter because only stlc has types. If a second typed language arrives, add a filter.

### Tests

- **qcheck generator for stlc is boolean-only** (depth-bounded over True/False/If). Doesn't exercise application or lambdas. A fuller generator would need to track types, which is straightforward but adds complexity. Worth doing if the translator grows.
- **Lc→Stlc qcheck property.** Could generate random simply-typable lc terms by forward-sampling from stlc, erasing, and checking that Lc→Stlc recovers a term with the original type. Worth considering.

### Store / content-addressing

- **Ty hash space.** p6 uses `Ty.hash` only for procedure-identity suffixes; ty hashes don't participate in `Definition.t` directly. Keeping ty hashes out of the Store is the decision; if that changes, Store would need a third `language` for types or a separate ty table.
- **Cross-language structural identity via translation.** If `Stlc.True` and `Lc(\t.\f. t)` both mean "true" in some sense, should they share anything at the Store level? No — translation links them via an aspect, and that's the design answer (same as p5's stance on arith↔lc).

### Cross-prototype

- **Migration back to `docs/design/`.** p6 candidates:
  - **Partial-translator representation** (`Translation_untypable(string)`) — the first worked example of "procedure refused this input." Belongs in `docs/design/05-translation.md` if it survives a second use.
  - **Translator with extra inputs via procedure-id suffix** — worked example for `05-translation.md`. Possibly revisit whether `Attachment.t` should formalize `extras`.
  - **Typed aspect values beyond `Hash.t`** (`Type_of(Ty.t)` is the first) — worth a note in `02-definitions-and-derived-data.md`.
  - **Store-level per-language validity invariant** ("every stored stlc definition type-checks") — complements p5's "no cross-language references" in `03-content-addressing.md` or `06-architecture.md`.

  None are forced; per `CLAUDE.md`, migrate when learnings actually refine the substrate design, not eagerly.
