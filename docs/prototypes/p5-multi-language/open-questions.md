# Open Questions — p5-multi-language

Prototype-specific open items. Distinguishes "positions taken here (see `decisions.md`)" from "intentionally deferred."

## Positions this prototype takes

- **Multi-language `Definition.t`** as a sum (`Arith | Lc`). Shape extensible to a third language with one variant + per-language module set.
- **Language-tag byte prefix** in Node encodings ('A', 'L'). Explicit byte-level separation of hash spaces.
- **Register-time cross-language reference rejection.** Store raises `Language_mismatch` rather than silently accepting mixed parents.
- **Resolver language guard** at edit time. User sees the mismatch at `:bind` / parse time, not only at ingest.
- **Translation stored as a derived aspect on the source**, with procedure identity `arith-to-lc-church:translate:v1`. Cache-hit on repeat.
- **Church encoding specifics** (TAPL-standard): `λt.λf. t/f`, Kleene PRED, `c t e` for `if`. Works; deep β-normalization agrees with arith evaluation on every small arith AST tested.
- **Mode-based REPL language switching** (`:lang arith` / `:lang lc`).
- **Language-labelled viewing** (`[arith]` / `[lc]   ` prefixes on `:list`, `:names`, `:dag`, `:lookup`, `:show`; split counts in `:stats`).

## Still open — intentionally deferred

### Translation

- **Reverse translator (`lc → arith`).** Not generally decidable. Could attempt a partial one (accept only terms that Church-decode to something arith-shaped), but this is scholastic; if it ever pays off, a future prototype takes it.
- **Second arith → lc translator** (e.g., Scott encoding). The substrate supports multiple translators per pair (`05-translation.md:§Translators as available actions`); p5 ships one. A second would let us test that the aspect cache's procedure-identity keying actually separates the two output sets. Small, worth doing once a second encoding has a reason to exist.
- **Automatic re-translation on source change; orphan-translation GC.** `05-translation.md:§Staleness` leaves this deferred. When the source definition changes, the old translation entry points at the old source hash — correct but dangling-looking. A later prototype decides whether to purge, preserve, or flag.
- **Translator certification / property-based validation.** `05-translation.md:§Future automation`. Noted, not attempted.
- **Declarative structural-embedding generator.** `05-translation.md:§Subset translations`. Arith is not a subset of lc (no Church encoding is purely structural), so this wouldn't even apply to this pair; mentioned for completeness.
- **Auto-binding a name for the translation target.** Right now `:translate foo` prints the target hash; the user runs `:bind foo_lc #<prefix>` to name it. A `--bind <name>` flag or a `:translate-and-bind` shortcut would be convenient. Deferred.
- **Lineage-only reverse aspect** on the target (`source-translation-from-arith`). `05-translation.md:§How translation integrates` notes it as optional; would enable "what arith source did this lc hash come from?" without the bidirectional-consistency headache. Skipped in p5 because `Attachment.by_value` already answers a narrower version of the question.

### Transitive dependencies

- **`Ref(hash)` in arith or lc.** p3 and p4 both inline at resolution; p5 inherits that. Stored arith and lc definitions are closed deep trees. When a future prototype introduces `Ref(hash)`, the translator's eager-closure walk becomes non-trivial: follow the ref, translate the dependency, substitute the target ref, hash-cons on overlap. Noted for when that prototype happens.

### Evaluators and normalization

- **No `Lc_deep_eval` module.** The tests use a `deep_normalize` helper inside `test_p5.re`. If future prototypes need normal-form reduction in the runtime (for a pretty-print-after-normalize mode, or a `:normalize` REPL command), it graduates. Not yet.
- **Step accounting on the translator.** The translator's β-reductions aren't counted; the lc evaluator's budget applies when the user later `:eval`s the target. Probably fine but worth noting if the budget turns out to be exhausted by translation tracing we didn't expect.

### Store / content-addressing

- **Language-tag bytes as registered IDs.** The byte-level tag is fine for two languages. If many languages arrive, a registry of `(tag_byte, language_name)` mappings avoids ad-hoc byte assignments. Deferred.
- **Cross-language registration errors beyond `Language_mismatch`.** Today we raise; a `result(_, Language_mismatch)` would be cleaner at the type level. Fine as-is; exception lets the REPL catch and print a message.
- **Cross-language structural identity via translation.** If `arith(one)` and `lc(c_one)` both represent "1" semantically, should they share something at the Store level? No — translation links them via an aspect, and that's the design answer. Worth rewriting if the question comes up again.

### Resolver

- **Namespace becomes crowded fast.** Loading the bootstrap script binds 17 names across both languages. Users will want `:namespace arith` / `:namespace lc` views. The `:names` output already labels entries with `[arith]` / `[lc]`, so filtering is textual (`:names | grep '\[arith\]'`). Adding a filter flag to `:names` is mechanical; not pressing.
- **Fresh-name alphabet** for the lc printer doesn't know about arith keywords. In principle `fresh_name` could pick `if` or `succ` as a binder display name, and the round-trip through the lc parser would see it as an identifier (not reserved — the lc lexer has no arith keywords). Low risk today; flag if it bites.

### Pretty-printing

- **Large Church terms in `:list closed`** are unreadable as strings. E.g. `branch`'s translation:
  `(\x. x (\y. c_zero) const) ((\x. \y. \z. x (\a. \b. b (a y)) (\a. z) id) ((\x. \y. \z. y (x y z)) c_zero)) c_zero ((\x. \y. \z. y (x y z)) ((\x. \y. \z. y (x y z)) c_zero))`.
  Legible because named children (`c_zero`, `const`, `id`) collapse, but still dense. A `:list closed --top-level-only` or a vertical pretty-print would help once we have more translations. Not urgent.
- **No special display for translation targets.** A translation target looks like any other lc definition in `:list`. A small `← translated from arith:<name>` annotation (via the optional reverse aspect) would improve legibility. Deferred with the reverse aspect itself.

### REPL UX

- **`:translate` requires a name or hash prefix, not a bare expression.** `:translate succ 0` doesn't work — you have to `:bind x succ 0` first, then `:translate x`. For a one-off "translate this arith expression" workflow, a `:translate-expr <expr>` that parses, ingests, and translates in one go would be convenient. Noted.
- **No `:untranslate` / flush.** Once translated, the aspect is Derived — immutable for that procedure identity. If the translator bug-fixes require re-translation, users bump the procedure version (`:v2`) and re-run. This is the design, but there's no REPL affordance for it yet. Deferred until it bites.

### Tests

- **qcheck generator for arith is bounded at depth 3.** Larger asts make the deep β-normalizer slow (Church predecessor is expensive). If we want to stress-test the translator harder, a dedicated normalizer that reduces WHNF-inside-binders without going full normal form would help.
- **No cross-language test for the Namespace.** Name shadowing across languages: bind `foo` to an arith hash, try to `:bind foo` in lc mode (hits `Name_already_bound` because Namespace doesn't care about languages). Tested only indirectly.

### Cross-prototype

- **Migration back to `docs/design/`.** p5 candidates:
  - The `Definition.t`-as-sum shape is the canonical multi-language answer; worth writing up in `02-definitions-and-derived-data.md` or `06-architecture.md`.
  - The "language-tag byte prefix" decision could become substrate guidance for any prototype with more than one language.
  - The "Resolver language guard" rule is the edit-time companion to the Store's registration check; worth a line in `04-naming-layer.md` once that doc exists / is updated.
  - The aspect-shape for translations (`Translation_target(Hash.t)` on the source) could become a worked example in `05-translation.md`.
  None are forced; the discipline (per `CLAUDE.md`) is "migrate back when it refines the substrate design," not eagerly.
