# Translation

**Status:** Draft.

## Purpose

Pin down how translations between languages work: what a translator is, what it produces, how results are cached as associated data, and where hand-written per-pair translators end and future automation could begin.

## What translation is

A **translator** is a procedure that takes a definition in a source language L1 and produces a new definition in a target language L2, with a recorded correspondence between them. Translation is directional; a translator name and its reverse are different procedures.

Translators are:

- **Per-pair.** Each directed `(L1, L2)` pair is served by one or more translators. There can be multiple translators per pair — different elaboration strategies, inlining policies, or levels of faithfulness.
- **Hand-written.** Each translator is code in the substrate's host language, mapping L1 ASTs to L2 ASTs. Automation is a future (see below), not a commitment.
- **Named and versioned.** Like any derived procedure (`02-definitions-and-derived-data.md`), a translator has an identity: a tag + manual version, e.g., `stlc-to-stlc+subtyping:translate:v1`.

Running a translator is an explicit action. There is no automatic translation on lookup, no implicit invocation at compile time.

## Translators as available actions

A translator is a **potential action** the substrate makes available to interfaces. For each directed `(L1, L2)` pair, the substrate can enumerate the translators registered for it. The substrate does not choose among them; interfaces do, by exposing them to the user or selecting via an interface-level policy.

Multiple translators per pair coexist naturally. Each is a distinct procedure with a distinct identity; their outputs are distinct derived aspects on the source definition. Running the same source through translator T1 versus T2 produces separate cache entries that do not compete.

This "translators as available actions" framing is specific to this doc only because translation is the first place multiple procedures can target the same logical job. The same pattern applies to any derived procedure (type checkers, evaluators, analyzers) when more than one is registered for a given aspect; we'll generalize only if and when it earns its keep.

## How translation integrates with the substrate

When translator T: L1 → L2 is run on a source definition D_L1 (hash `H1`):

1. T produces a new definition D_L2 whose body is entirely in L2 (no cross-language references). This gets stored normally with its own hash `H2`. It is a first-class L2 definition from that point on.
2. An aspect entry is recorded on D_L1: `aspect = translation-to-L2:<translator-identity>`, `value = H2`. This is the cached correspondence.
3. (Optionally) a reverse aspect entry may be recorded on D_L2 pointing back to `H1` as the source. Useful for lineage display, not required by the substrate.

The aspect entry is the cache. If T is run again on D_L1 with the same translator identity, we return `H2` from the aspect store without recomputing. Multiple translators from L1 can produce multiple aspect entries — one per translator identity — coexisting without conflict.

Translations are always **derived aspects**: they are produced by a named procedure and immutable for that procedure's identity. Bumping the translator's version produces a new aspect, not a mutation of the existing one. This reuses the procedure-identity machinery from `02`; no translation-specific caching is introduced.

## Transitive dependencies

If D_L1 references another L1 definition E_L1 (hash `He1`), D_L2 cannot reference `He1` (no cross-language references). D_L2 must reference some E_L2 in L2.

**Translation is eager over the transitive closure.** Running translator T on D_L1 translates D_L1 and every L1 definition it transitively depends on, producing a matched set of L2 definitions. The substrate does not support partial or lazy translation.

For each dependency encountered during a translation run, the substrate consults the aspect store: if the dependency has a cached translation under the same translator identity, the existing L2 hash is used directly — no re-computation. This makes repeated translation over overlapping closures mostly cache hits after the first run.

Translator correctness is the translator's responsibility: the translator must ensure that, when D_L2's body references an E_L2 hash, that E_L2 is the translation of the E_L1 that D_L1 referenced. The substrate enforces the "no cross-language references" rule on the outputs (`03-content-addressing.md`); beyond that, semantic correctness is the translator's responsibility.

## Subset translations

When L1 is structurally a subset of L2 — every L1 AST shape is a valid L2 AST shape — the "translation" is almost trivial: lift each node to its L2 equivalent, change the definition's `language` property, compute new hashes. We still write such translators by hand; they're short.

A declarative **"structural embedding"** generator — a spec saying "L1 embeds in L2 at these nodes" from which the substrate produces the translator — would be natural future work, but we are not building it.

## Bidirectionality

- A translator is one-directional. L1 → L2 and L2 → L1 are separate translators with separate identities.
- Roundtripping (`L1 → L2 → L1`) is **not guaranteed.** Translation may be lossy in either direction. Subset relations give a retract (L2 → L1 may be partial, but L1 → L2 → L1 is identity on the L1-shaped fragment of L2) but not in general a section.
- The substrate does not check bidirectional consistency. If two translators claim to be inverses, that's a property a future aspect could record — not a built-in guarantee.

## Staleness

When D_L1's source changes, the translator isn't re-invoked automatically. The previous translation cache entry is still a valid, correct record — it's just a translation of a definition no one currently has a name for.

- The entry points to an **orphan source hash** (the old D_L1). That hash still exists in the store (content-addressed immutability).
- The new D_L1 has no translation cache entry yet; running the translator produces a new L2 definition with its own hash.

Garbage collection — whether to purge orphan translations, preserve them as history, or leave them untouched — is deferred. For now, orphan entries accumulate in the store.

## Future automation (noted, not committed)

In the current phase we **trust hand-written translators to be correct.** There is no verification, no certification, no property-based testing infrastructure in the substrate. This is the same discipline bet we make for procedure identity generally (`02-definitions-and-derived-data.md`).

Directions worth keeping on the research list:

- **Cambria-style schema lenses.** The Cambria model — bidirectional, composable schema migrations — maps interestingly onto translations between closely related languages (or between versions of the same language). Strengths: structural mappings and composition. Weaknesses: semantic properties and evaluation. Worth studying once we have enough hand-written translators to see the patterns. Citations added 2026-08-03: Litt, van Hardenberg & Henry, "Project Cambria: Translate your data with lenses," Ink & Switch, October 2020; the academic basis is Hofmann, Pierce & Wagner, "Edit Lenses," POPL 2012, over Foster et al., TOPLAS 2007. Two divergences worth noting before adopting the framing — Cambria's lenses are **bidirectional** where our translators are explicitly one-directional (`decisions.md`), and they operate on **edits** where ours operate on whole definitions. Its schema-graph-plus-shortest-path composition is, however, the prior art for the transitive-translation policy already settled above. See `../related-work/06-multi-language.md` §"Lenses and schema migration — the Cambria line."
- **Proof-assisted translation.** A translator produces a machine-checkable certificate that its output is equivalent to its input under some relation. The substrate can carry the certificate as an aspect entry and accept verified translations without re-checking. Heavy machinery; only pays off when translation correctness matters more than translation velocity.
- **LLM-assisted translation.** An LLM proposes a translation; validation comes from a test suite, a certificate, user review, or all three. Low floor, high ceiling, bad failure modes. Noted because it's the shape of many plausible future tools.
- **Property-based testing of translators.** Lighter weight than certification: generate L1 inputs, run the translator, check that some property (evaluation agreement, type preservation, roundtripping on a subset) holds. An option if certification feels too heavy but we want a correctness gradient.

None of these is on the roadmap. Each is a future research/engineering investment we may or may not make.

## Non-goals (current phase)

- **Automatic translation inference.** Translations are explicit, invoked by hand.
- **Guaranteed roundtripping.** Not enforced, not checked.
- **Cross-language dependency resolution.** D_L2 references only L2 definitions. If a dependency doesn't exist in L2, the translator fails or translates it too; the substrate has no special mechanism.
- **Translation completeness checking.** No attempt to verify that a translator covers every L1 construct. Missing cases fail at runtime.
- **Bidirectional lens infrastructure.** Cambria-style is noted as a future, not built.

## Open sub-questions

The design sub-questions raised during initial work on this doc have all been either resolved in `decisions.md` or explicitly deferred. See the relevant sections above for deferrals (garbage collection of orphan translations; certification infrastructure; declarative subset-embedding generator). New sub-questions will accumulate here as implementation work surfaces them.
