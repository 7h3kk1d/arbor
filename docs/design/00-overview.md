# Overview

**Status:** Draft. Personal design exploration, intended to graduate into a research prospectus.
**Project name:** **arbor**, Latin for *tree*, after the content-addressed DAG of definitions and the trees its aspect stores (namespaces, type derivations, translation lineages) grow over it. The working directory was renamed to `arbor` on 2026-06-01.

## Vision

A content-addressed computational substrate for defining, composing, and translating between a growing family of small programming languages. Languages are introduced incrementally, following Pierce's *Types and Programming Languages* (TAPL). All definitions share one content-addressed space, and each belongs to exactly one language. Languages relate through explicit, cached translations, not a shared AST or runtime.

The longer-term goal is a candidate substrate for the *computational commons* sketched in Hazel's propl24 paper. `07-hazel-substrate.md` collects the Hazel ideas, papers, and artifacts that feed this design.

## Core model at a glance

- **Definitions are per-language and content-addressed.** Each definition belongs to one language and is identified by the hash of its canonical form.
- **Associated data is uniform and extensible.** Names, types, translations, evaluation results, documentation, complexity analyses: anything attached to a definition lives in one store, tagged by **aspect**. Aspects are asserted (human-authored, mutable) or derived (computed by a known procedure). See `02-definitions-and-derived-data.md`.
- **Calls are content-addressed.** A definition names the definitions it uses by hash. Names never appear in the stored program.
- **No cross-language references.** An L1 definition cannot cite an L2 definition. Reusing L2 code from L1 means translating it into a fresh L1 definition with its own hash.
- **Translations are first-class, per-pair, and hand-written.** A translation L1 → L2 produces a new L2 definition and records the correspondence as associated data on the source; that record doubles as a cache. Automated and semi-automated translation (Cambria-style schema migration, proof-assisted, LLM-assisted) is noted as future work.
- **Names are an editing-layer concern.** Binding, renaming, and eventually branching happen in a layer separate from the program store. Names resolve to hashes at edit time.
- **No substrate-level static/dynamic divide.** Whether a language has a type system, an evaluation relation, or neither (e.g., markup) depends on which aspects it registers for. The substrate privileges no combination.
- **Languages are added incrementally.** The roadmap follows TAPL: untyped λ → simply typed → extensions → subtyping → references → parametric polymorphism → recursive types → … Each new language is additive and leaves existing ones undisturbed.

## Architecture

Four layers with upward-only dependencies; details in `06-architecture.md`.

1. **Store**: content-addressed storage of typed `Definition.t` values keyed by hash. Enforces "no cross-language references" at registration.
2. **Attachment**: the aspect store and namespace(s). Typed associated-data entries keyed by hash, plus name → hash bindings, with bidirectional queries.
3. **Language**: per-language modules (AST, canonicalizer, type-check, evaluator, primitives) and inter-language translators. Pure functions consumed by Store and interfaces.
4. **Interface**: user-facing modalities, plural by design. The substrate assumes no single editor, file format, or user model.

The lower three layers form the computational core and expose a stable, interface-agnostic API.

## Non-goals (current phase)

- **Effect systems** at the language layer. Deferred indefinitely.
- **ABI stability across hash changes.** Changing a definition produces a new hash, and callers do not migrate automatically. Unison's migration UX is prior art (see `03-content-addressing.md`) but is not inherited.
- **Distribution and networked code sharing.** Unison's flagship capability; out of scope.
- **Branching, merging, namespace versioning.** Part of the long-term vision, deferred until the single-user model is clear.
- **Performance benchmarking.** Rough scalability only; no latency or throughput targets.

## Strategic posture

Step in slowly. Prefer the narrow, concrete implementation over the general one. Composition infrastructure (shared IRs, feature lattices, modular type systems) waits until per-language, per-pair machinery becomes too costly. `01-language-model.md` lays out the option space being held open.

## Glossary

- **Language**: a syntax + semantics pair. A definition belongs to exactly one language; the store is shared by all of them.
- **Definition**: a hash-identified program fragment in one language.
- **Hash**: the canonical content identifier of a definition.
- **Name**: an editing-layer label that resolves to a hash. Not part of stored programs.
- **Associated data**: anything attached to a definition other than its body. Stored uniformly, tagged by aspect.
- **Aspect**: a category of associated data (e.g., `static-type-check`, `evaluation`, `translation-to-L2`, `name`, `description`). Asserted or derived.
- **Asserted**: an aspect whose entries are human-authored and mutable.
- **Derived**: an aspect whose entries are produced by a named procedure and are immutable for a fixed procedure identity.
- **Procedure identity**: the stable handle under which derived entries are cached. Near-term, a tag plus manual version; far-term, content-addressed procedures.
- **Translation**: a per-pair, hand-written procedure producing a definition in one language from a definition in another. Recorded as derived associated data on the source.
- **Interface**: any user-facing modality for authoring, browsing, or executing programs.
- **Computational core**: the interface-agnostic storage, reasoning, and translation engine.

## Document map

- `00-overview.md`: this file.
- `01-language-model.md`: what a "language" is; composition options A–D; workbench landscape notes.
- `02-definitions-and-derived-data.md`: the ontology. Definitions, associated data, aspects, asserted vs. derived, procedure identity.
- `03-content-addressing.md`: hashing, calls-by-hash, Unison ABI notes, primitive identity.
- `04-naming-layer.md`: Unison-style names over hashes; branching deferred.
- `05-translation.md`: per-pair translators, caching, staleness, future automation.
- `06-architecture.md`: the four-layer decomposition (Store, Attachment, Language, Interface), dependency rules, pure vs. stateful API tiers.
- `07-hazel-substrate.md`: sourcing farm for Hazel-specific requirements, solutions, and artifacts.
- `08-tech-stack.md`: OCaml reuse vs. alternatives.
- `09-roadmap.md`: prioritization, first prototype.
- `10-minted-identity.md`: an opt-in axis of identity, orthogonal to content addressing, for definitions that should not collapse with coincidental structural twins.
- `11-label-sort.md`: labels as a first-class minted sort, decoupling record/module field identity from the human-readable field name.
- `12-type-abstraction.md`: abstract types as `Type` definitions identified by `opaque ++ mint ++ witness`; signatures as existential/universal binders over label-records; sealing, functors, and the projection-inlining dependency model that keeps abstraction fine-grained.
- `13-editing-context.md`: *notes, 2026-08-19.* The editing context, or working set: a named collection of definitions in flight with no commit gate (everything is ingested; only *names* stay uncommitted), the frontier-vs-commit-set split, the context as evaluation environment, and the context as the scope of migration.
- `14-capabilities.md`: *goals, 2026-09-09.* The capabilities arbor must provide, stated from a user's point of view: reference and naming, working on incomplete code, changing definitions and adopting others' changes, reusing work already done, reviewing and undoing change, and plurality of languages and actors. Requirements only, not a status report.
- `decisions.md`: ADR-lite log of commitments, with dates and rationale.
- `open-questions.md`: running list; items graduate to docs or decisions as they resolve.
- `prototype-findings.md`: stocktake across the prototype series. Recurring patterns, single-prototype experiments, and shapes left open. Observational, not prescriptive.

Alongside this directory:

- `../related-work/00-index.md`: themed index of the literature this design sits next to, with verification tags, a ranked priority-read list, and recorded negative findings. Backward-looking; complements the forward-looking sourcing in `07-hazel-substrate.md`.
- `../systems/00-index.md`: source-level reads of other implementations, pinned to a commit instead of cited to a DOI. Several of the closest systems have no publication, so their source is the only citable artifact. First read: `../systems/unison/`.
