# Overview

**Status:** Draft. Personal design exploration, intended to graduate into a research prospectus.
**Project name:** **arbor** — Latin for *tree*, after the content-addressed DAG of definitions and the trees its aspect stores (namespaces, type derivations, translation lineages) grow over it. The working directory was renamed to `arbor` on 2026-06-01.

## Vision

A content-addressed computational substrate for defining, composing, and translating between a growing family of small programming languages. Languages are introduced incrementally, following the progression in Pierce's *Types and Programming Languages* (TAPL). Definitions are stored in a single content-addressed space — each definition belongs to exactly one language — and relationships between languages are mediated by explicit, cached translations rather than shared AST or shared runtime.

The longer-term goal is to serve as a candidate substrate for the *computational commons* sketched in Hazel's propl24 paper. See `07-hazel-substrate.md` for how Hazel-specific ideas, papers, and artifacts feed into this design.

## Core model at a glance

- **Definitions are per-language and content-addressed.** Each definition belongs to exactly one language and is identified by the hash of its canonical form.
- **Associated data is uniform and extensible.** Anything attached to a definition — names, types, translations, evaluation results, documentation, complexity analysis — lives in a single unified store, tagged by **aspect**. Aspects are either asserted (human-authored, mutable) or derived (computed by a known procedure). See `02-definitions-and-derived-data.md`.
- **Calls are content-addressed.** A definition that uses another names it by hash, not by identifier. Names never appear in the stored program.
- **No cross-language references.** An L1 definition cannot cite an L2 definition. To reuse an L2 definition from L1, you translate it, producing a fresh L1 definition with its own hash.
- **Translations are first-class, per-pair, and hand-written.** A translation L1 → L2 produces a new L2 definition and records the correspondence as associated data on the source. That record doubles as a cache. Automated and semi-automated translation (Cambria-style schema migration, proof-assisted, LLM-assisted) is noted as a future, not a commitment.
- **Names are an editing-layer concern.** Binding, renaming, and eventually branching happen in a layer separate from the program store. Names resolve to hashes at edit time.
- **No substrate-level static/dynamic divide.** Whether a language has a type system, an evaluation relation, or neither (e.g., a markup language) is a matter of which aspects it registers for. The substrate does not privilege any combination.
- **New languages are added incrementally.** The roadmap follows TAPL: untyped λ → simply typed → extensions → subtyping → references → parametric polymorphism → recursive types → … Each new language is additive; existing ones are not disturbed.

## Architecture

Four layers, with strict upward-only dependencies. See `06-architecture.md` for details.

1. **Store** — content-addressed definition storage. Holds typed `Definition.t` values keyed by hash; enforces "no cross-language references" at registration.
2. **Attachment** — aspect store and namespace(s). Holds typed associated-data entries keyed by hash; holds name → hash bindings. Bidirectional queries.
3. **Language** — per-language modules (AST, canonicalizer, type-check, evaluator, primitives) plus inter-language translators. Pure functions consumed by Store and interfaces.
4. **Interface** — user-facing modalities. Plural by design. The substrate never assumes one editor, one file format, or one user model.

The lower three layers together form the computational core and expose a stable, interface-agnostic API.

## Non-goals (current phase)

- **Effect systems** at the language layer — deferred indefinitely.
- **ABI stability across hash changes.** Changing a definition produces a new hash; callers do not migrate automatically. Unison's migration UX is noted as prior art (see `03-content-addressing.md`) but not inherited.
- **Distribution and networked code sharing.** Unison's flagship capability, explicitly out of scope.
- **Branching, merging, namespace versioning.** Part of the long-term vision, deferred until the single-user model is clear.
- **Performance benchmarking.** Rough scalability only; no latency or throughput targets.

## Strategic posture

Step in slowly. When in doubt, prefer the narrow, concrete implementation over the general, abstract one. Composition infrastructure (shared IRs, feature lattices, modular type systems) is deferred until the per-language and per-pair approach becomes prohibitively friction-heavy. See `01-language-model.md` for the option space we're explicitly holding open.

## Glossary

- **Language** — a distinct syntax + semantics pair. A definition belongs to exactly one language; the store itself is shared across all languages.
- **Definition** — a hash-identified program fragment belonging to exactly one language.
- **Hash** — canonical content identifier for a definition.
- **Name** — an editing-layer label that resolves to a hash. Not part of stored programs.
- **Associated data** — anything attached to a definition that is not its body. Stored uniformly, tagged by aspect.
- **Aspect** — a category of associated data (e.g., `static-type-check`, `evaluation`, `translation-to-L2`, `name`, `description`). Asserted or derived.
- **Asserted** — a property of an aspect whose entries are human-authored and mutable.
- **Derived** — a property of an aspect whose entries are produced by a named procedure and immutable for a fixed procedure identity.
- **Procedure identity** — the stable handle by which the substrate caches derived entries. Near-term: tag + manual version. Far-term: content-addressed procedures.
- **Translation** — a per-pair, hand-written procedure producing a definition in one language from a definition in another. Recorded as derived associated data on the source.
- **Interface** — any user-facing modality for authoring, browsing, or executing programs.
- **Computational core** — the storage + reasoning + translation engine, interface-agnostic.

## Document map

- `00-overview.md` — this file.
- `01-language-model.md` — what is a "language"; composition options A–D; workbench landscape notes.
- `02-definitions-and-derived-data.md` — the ontology: definitions, associated data, aspects, asserted vs. derived, procedure identity.
- `03-content-addressing.md` — hashing, calls-by-hash, Unison ABI notes, primitive identity.
- `04-naming-layer.md` — Unison-style names over hashes; branching deferred.
- `05-translation.md` — per-pair translators, caching, staleness, future automation.
- `06-architecture.md` — four-layer decomposition (Store, Attachment, Language, Interface), dependency rules, pure vs. stateful API tiers.
- `07-hazel-substrate.md` — sourcing farm for Hazel-specific requirements, solutions, and artifacts.
- `08-tech-stack.md` — OCaml reuse vs alternatives.
- `09-roadmap.md` — prioritization, first prototype.
- `10-minted-identity.md` — opt-in axis of identity, orthogonal to content addressing, for definitions that should not collapse with coincidental structural twins.
- `11-label-sort.md` — labels as a first-class minted sort, decoupling record/module field identity from the human-readable field name.
- `12-type-abstraction.md` — abstract types as `Type` definitions identified by `opaque ++ mint ++ witness`; signatures as existential/universal binders over label-records; sealing, functors, and the projection-inlining dependency model that keeps abstraction fine-grained.
- `decisions.md` — ADR-lite log of commitments, with dates and rationale.
- `open-questions.md` — running list; items graduate to docs or decisions as they resolve.
- `prototype-findings.md` — stocktake across the prototype series: recurring patterns, single-prototype experiments, and shapes left open. Observational, not prescriptive.

Alongside this directory: `../related-work/00-index.md` — themed index of the literature this design already sits next to, with verification tags, a ranked priority-read list, and recorded negative findings. Backward-looking; complements `07-hazel-substrate.md`'s forward-looking sourcing farm.
