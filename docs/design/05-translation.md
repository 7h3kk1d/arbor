# Translation

**Status:** Draft.

## Purpose

How translations between languages work: what a translator is, what it produces, how results are cached as associated data, and where hand-written per-pair translators end and automation could begin.

## What translation is

A **translator** is a procedure that takes a definition in a source language L1 and produces a new definition in a target language L2, recording the correspondence. Translation is directional: a translator and its reverse are different procedures.

Translators are:

- **Per-pair.** Each directed `(L1, L2)` pair is served by one or more translators, which may differ in elaboration strategy, inlining policy, or faithfulness.
- **Hand-written.** Each translator is host-language code mapping L1 ASTs to L2 ASTs. Automation is future work (below).
- **Named and versioned.** Like any derived procedure (`02-definitions-and-derived-data.md`), a translator has an identity, a tag plus manual version such as `stlc-to-stlc+subtyping:translate:v1`.

Running a translator is an explicit action. Nothing translates automatically on lookup or implicitly at compile time.

## Translators as available actions

A translator is a potential action the substrate offers to interfaces. For each directed `(L1, L2)` pair the substrate can enumerate the registered translators. It does not choose among them; interfaces do, by exposing the choice to the user or applying an interface-level policy.

Several translators per pair coexist. Each has its own identity, so its outputs are distinct derived aspects on the source definition, and running one source through T1 and T2 produces separate cache entries that do not compete.

Translation is simply the first place where several procedures target the same job. The pattern applies to any derived procedure (type checkers, evaluators, analyzers) once more than one is registered for an aspect, and will be generalized only when that pays off.

## How translation integrates with the substrate

When translator T: L1 → L2 runs on a source definition D_L1 with hash `H1`:

1. T produces a new definition D_L2 whose body is entirely in L2, with no cross-language references. It is stored normally under its own hash `H2` and is a first-class L2 definition from then on.
2. An aspect entry is recorded on D_L1: `aspect = translation-to-L2:<translator-identity>`, `value = H2`. This is the cached correspondence.
3. Optionally, a reverse entry on D_L2 points back to `H1`. It helps lineage display and is not required by the substrate.

The aspect entry is the cache. Running T again on D_L1 under the same identity returns `H2` from the aspect store without recomputing. Translators from L1 produce one entry per translator identity, and these coexist without conflict.

Translations are always **derived aspects**, immutable for the producing procedure's identity. Bumping a translator's version produces a new aspect, not a mutation of the old one. This reuses the procedure-identity machinery from `02`; there is no translation-specific caching.

## Transitive dependencies

If D_L1 references another L1 definition E_L1 (hash `He1`), D_L2 cannot reference `He1`, because there are no cross-language references. It must reference some E_L2.

**Translation is eager over the transitive closure.** Running T on D_L1 translates D_L1 and every L1 definition it transitively depends on, producing a matched set of L2 definitions. Partial or lazy translation is not supported.

For each dependency, the substrate consults the aspect store. If the dependency already has a translation under the same translator identity, the existing L2 hash is reused. Repeated translation over overlapping closures is therefore mostly cache hits after the first run.

Correctness belongs to the translator. When D_L2 references an E_L2 hash, that E_L2 must be the translation of the E_L1 that D_L1 referenced. The substrate enforces only the no-cross-language-references rule on outputs (`03-content-addressing.md`).

## Subset translations

When L1 is structurally a subset of L2, so that every L1 AST shape is a valid L2 shape, translation is almost trivial: lift each node to its L2 equivalent, change the definition's `language` property, and compute new hashes. Such translators are still written by hand; they are short.

A declarative **structural-embedding** generator, a spec saying "L1 embeds in L2 at these nodes" from which the substrate derives the translator, would be natural future work. It is not being built.

## Bidirectionality

- A translator is one-directional. L1 → L2 and L2 → L1 are separate translators with separate identities.
- Roundtripping (`L1 → L2 → L1`) is **not guaranteed**; translation may be lossy in either direction. A subset relation gives a retract (L2 → L1 may be partial, but L1 → L2 → L1 is the identity on the L1-shaped fragment of L2), but not in general a section.
- The substrate does not check bidirectional consistency. That two translators are inverses is a property a future aspect could record.

## Staleness

When D_L1's source changes, the translator is not re-invoked. The old cache entry remains a valid record: it translates a definition that no current name points to.

- The entry points to an **orphan source hash**, the old D_L1, which still exists in the immutable store.
- The new D_L1 has no translation entry yet; running the translator produces a new L2 definition with its own hash.

Whether to purge orphan translations, keep them as history, or leave them is deferred. For now they accumulate.

## Future automation (noted, not committed)

In this phase, hand-written translators are **trusted to be correct**. The substrate has no verification, certification, or property-based testing infrastructure. This is the same discipline bet made for procedure identity generally (`02-definitions-and-derived-data.md`).

Directions to keep on the research list:

- **Cambria-style schema lenses.** Cambria's bidirectional, composable schema migrations map onto translations between closely related languages, or between versions of one language. Its strengths are structural mappings and composition; its weaknesses are semantic properties and evaluation. Worth studying once enough hand-written translators exist to show patterns. Citations (added 2026-08-03): Litt, van Hardenberg & Henry, "Project Cambria: Translate your data with lenses," Ink & Switch, October 2020; the academic basis is Hofmann, Pierce & Wagner, "Edit Lenses," POPL 2012, building on Foster et al., TOPLAS 2007. Two divergences matter before adopting the framing: Cambria's lenses are **bidirectional** where our translators are one-directional (`decisions.md`), and they operate on **edits** where ours operate on whole definitions. Its schema-graph-plus-shortest-path composition is prior art for the transitive-translation policy above. See `../related-work/06-multi-language.md` §"Lenses and schema migration — the Cambria line."
- **Proof-assisted translation.** A translator emits a machine-checkable certificate that its output is equivalent to its input under some relation. The substrate can carry the certificate as an aspect entry and accept verified translations without re-checking. Heavy machinery, worthwhile only when translation correctness matters more than velocity.
- **LLM-assisted translation.** An LLM proposes a translation, validated by a test suite, a certificate, user review, or all three. Low floor, high ceiling, bad failure modes. Many plausible future tools have this shape.
- **Property-based testing of translators.** Lighter than certification: generate L1 inputs, translate, and check a property (evaluation agreement, type preservation, roundtripping on a subset). A correctness gradient if certification is too heavy.

None of these is on the roadmap.

## Non-goals (current phase)

- **Automatic translation inference.** Translations are invoked by hand.
- **Guaranteed roundtripping.** Neither enforced nor checked.
- **Cross-language dependency resolution.** D_L2 references only L2 definitions. If a dependency does not exist in L2, the translator fails or translates it too; the substrate has no special mechanism.
- **Translation completeness checking.** Nothing verifies that a translator covers every L1 construct. Missing cases fail at runtime.
- **Bidirectional lens infrastructure.** Cambria-style lenses are noted above, not built.

## Open sub-questions

The sub-questions raised while drafting this doc have been resolved in `decisions.md` or deferred in the sections above (garbage collection of orphan translations, certification infrastructure, the declarative subset-embedding generator). New ones will accumulate here as implementation surfaces them.
