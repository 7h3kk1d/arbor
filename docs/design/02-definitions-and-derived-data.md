# Definitions and Associated Data

**Status:** Draft.

## Purpose

The system's core ontology: what is stored, what is computed about it, and how the two relate. Language features, translation, naming, and interfaces all build on it.

The model is meant to keep unnecessary dichotomies out of the substrate, in particular the static-vs-dynamic typing divide and the split between languages with evaluation semantics and those without (e.g., markup). Both fall out of the model below.

## Core concepts

### Definition

The primary stored artifact: a hash-identified program fragment in exactly one language. Everything else in the system is about definitions.

### Associated data

Anything attached to a definition other than its body: descriptions, type annotations, inferred types, translation outputs, evaluation traces, complexity analyses, documentation, compiled forms. All of it sits in one store, tagged by **aspect**. Names are a close cousin but live in the naming layer; see "Kinship with the naming layer" below.

A definition can carry any number of entries across any number of aspects, including several within one aspect (e.g., independently computed types, or translations to different target languages).

### Aspect

A category of associated data. Each aspect declares:

- a stable identifier;
- whether its entries are **asserted** or **derived**;
- which languages it applies to;
- for derived aspects, the procedure that computes entries and that procedure's identity.

The term was chosen over "kind", which is reserved for the type-theory concept that will appear when we reach F-omega.

## Asserted vs. derived

One store, with the distinction made per aspect. There are not two subsystems; an aspect's declaration says whether its entries are authoritative facts or reconstructible outputs.

| | Asserted | Derived |
|---|---|---|
| **Source** | Human-authored | Produced by a known procedure applied to the definition (and possibly other associated data) |
| **Mutability** | Mutable; may be edited over time | Immutable for a fixed procedure identity; new versions appear as new entries |
| **Correctness** | True by fiat (until the author changes it) | Correct by construction for the procedure that produced it; stale ≠ wrong |
| **Reconstructible** | No | Yes, given the same procedure identity |
| **Examples** | Descriptions, tags, programmer-written type annotations, attached documentation | Inferred types, translation outputs, evaluation results, complexity analyses |

One logical property, such as "the type of this definition", may appear under both an asserted and a derived aspect. The programmer asserts an annotation, the type checker derives a type, and the system records both and can relate them (e.g., the check succeeded against the assertion). See the conflict-handling sub-question below.

**Two-way door.** A unified store can later be partitioned by aspect. Separate stores would need a migration to merge. Unified is the more reversible choice.

## Extensibility

A new aspect is introduced by registering an **aspect descriptor** carrying the fields listed under "Aspect" above. No schema change is needed, and a language can opt into aspects when it is introduced or later.

This is how the substrate avoids the static/dynamic divide. Typed languages register for `static-type-check`; untyped ones do not. A markup language registers for neither `evaluation` nor `static-type-check`, but may register for structural, documentation, or rendering aspects. "Has a type system" and "has evaluation semantics" are facts about participation, not substrate switches.

## Procedure identity

Derived entries are produced by procedures: type checkers, evaluators, translators, analyzers. A derived entry is cached under `(definition-hash, aspect, procedure-identity)`, so procedures need stable identities.

### Near-term — tag + manual version

A procedure is named by a human-assigned tag and version: `stlc:type-check:v1`, `stlc-to-stlc+subtyping:translate:v1`. When its behavior changes, the version is bumped by hand. Entries whose `procedure-identity` no longer matches a registered procedure are stale and may be recomputed or garbage-collected.

**Cost.** This requires discipline: changing a procedure without bumping its version makes the cache return wrong answers. That is acceptable during bootstrap. The alternative, automatic identity by content addressing, presupposes a meta-language we are not building yet.

### Far-term — procedures as content-addressed definitions

Procedures eventually become definitions in some meta-language, identified by their content hash, which makes cache invalidation automatic and precise. This is the metacircular bootstrap step. It is hard and deferred.

### Two-way door

The cache key's `procedure-identity` field is opaque from the start, so swapping tag+version for content hashes is an infrastructure change, not a semantic one.

## Kinship with the naming layer

The naming layer (`04-naming-layer.md`) has the same structure as the aspect store: both hold assertions relating strings or values to definition hashes.

- The aspect store keys entries by definition hash, with aspect-tagged values: *this definition has this type, this translation, this complexity*.
- The namespace keys entries by name, with definition hashes as values: *this name binds to this definition*.

Both need **bidirectional lookup**. Interfaces ask "what is this definition's X?" and also "which definitions have X?": hash → names for display, type → hashes for type-based search, translation source → translations.

Both will accrue **per-entry invariants**, and different ones: at most one hash per name in a namespace; perhaps at most one derived type per procedure identity in the aspect store; many-to-many for translations. Each store's invariants follow its entry shape and will grow on their own schedule.

The two stay separate systems for now. Unifying their lifecycle machinery is worth revisiting when branching and history arrive, since that is where a shared abstraction would pay for itself. `decisions.md` records the deferral.

## Implications for languages

- A language need not participate in every aspect. Typed languages register for typing aspects, evaluatable languages for evaluation aspects, and a markup or specification language may register only for structural or documentation aspects.
- Translation outputs sit in a translation aspect whose entries reference a new definition in the target language. "Translation produces a new definition with its own hash" and "translation is associated data on the source" are consistent: the associated datum is the record that the target definition exists.
- Evaluation traces, if kept, are derived associated data. Whether to keep them is an interface and storage question.

## Non-goals (current phase)

- Procedure content-addressing. Deferred as bootstrap.
- Aspect lifecycle management (deprecation, renaming, migration). Aspects are added, not yet removed.
- Access control on asserted entries. Single-user model assumed.

## Open sub-questions

Tracked in `open-questions.md` under "Associated data."

- **Vocabulary for "aspect".** Chosen provisionally to avoid the TAPL collision with "kind". Alternatives if friction appears: *facet*, *annotation*, *attribute*, *channel*.
- **Conflict handling.** When an asserted and a derived entry for the same logical property disagree (annotation says `Int→Int`, inference says `Bool→Bool`), the substrate records both. What does it surface? Probably a conflict to the interface; to be worked out when first encountered.
- **Where aspect descriptors live.** Per-language, a global registry, or a mix? Feeds into `08-tech-stack.md`.
- **Cross-aspect dependencies.** Evaluation needs types; translation may need elaboration. Are these formalized as aspect-graph edges or handled ad hoc in each procedure?
- **History of asserted entries.** Descriptions and annotations change. Is edit history kept, and at what granularity? Ties into the eventual branching story for both the aspect store and the naming layer.
- **Per-aspect uniqueness invariants.** Each aspect will likely want its own constraints (at most one entry per procedure identity for inferred types; none for translations). For now this is informal; eventually the aspect descriptor will likely declare the invariant.
