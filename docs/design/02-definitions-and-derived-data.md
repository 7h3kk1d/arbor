# Definitions and Associated Data

**Status:** Draft.

## Purpose

Establish the system's core ontology: what is stored, what is computed about it, and how these relate. This is the substrate that language-specific features, translation, naming, and interfaces all build on.

A main intent of this doc is to avoid baking any unnecessary dichotomy into the substrate — in particular the static-vs-dynamic typing divide and the distinction between languages that do and don't have evaluation semantics (e.g., markup). Both fall out naturally from the model below.

## Core concepts

### Definition

The primary stored artifact: a hash-identified program fragment in exactly one language. Everything else in the system is *about* definitions.

### Associated data

Anything attached to a definition that is not the definition's body. Descriptions, type annotations, inferred types, translation outputs, evaluation traces, complexity analyses, documentation, compiled forms — all sit in a single unified store, tagged by **aspect**. (Names are a close cousin but live in the naming layer rather than here; see "Kinship with the naming layer" below.)

A definition can carry any number of associated-data entries, across any number of aspects, including multiple entries within a single aspect (e.g., several independently computed types, or translations to different target languages).

### Aspect

A category of associated data. Each aspect declares:

- Whether entries in it are **asserted** or **derived**.
- Which languages it applies to.
- How it's computed, if derived.
- Its stable identifier.

Aspects are extensible: new ones can be added without schema changes. A language opts into the aspects it participates in when it is introduced, or later.

(Chosen over "kind" to avoid collision with kind-in-type-theory, which will appear literally when we reach F-omega.)

## Asserted vs. derived

Single store, aspect-level distinction. We don't build two subsystems; we know from an aspect's declaration whether its entries are authoritative facts or reconstructible outputs.

| | Asserted | Derived |
|---|---|---|
| **Source** | Human-authored | Produced by a known procedure applied to the definition (and possibly other associated data) |
| **Mutability** | Mutable; may be edited over time | Immutable for a fixed procedure identity; new versions appear as new entries |
| **Correctness** | True by fiat (until the author changes it) | Correct by construction for the procedure that produced it; stale ≠ wrong |
| **Reconstructible** | No | Yes, given the same procedure identity |
| **Examples** | Descriptions, tags, programmer-written type annotations, attached documentation | Inferred types, translation outputs, evaluation results, complexity analyses |

A single logical property (e.g., "the type of this definition") may appear under both an asserted and a derived aspect. The programmer asserts an annotation; the type checker derives a type; the system records both and can relate them (e.g., the check succeeded against the assertion). See the disagreement sub-question below.

**Two-way door.** Starting unified lets us partition later by aspect if it becomes useful. Starting separate would require migration if we ever wanted to merge. Unified wins on reversibility.

## Extensibility

A new aspect is introduced by registering an **aspect descriptor** with:

- A stable identifier.
- Asserted or derived.
- The languages it applies to.
- For derived aspects, the procedure that produces entries and its identity.

This is how the substrate avoids the static/dynamic typing divide: typed languages register for `static-type-check`; untyped ones don't. A markup language registers for neither `evaluation` nor `static-type-check`, but may register for structural, documentation, or rendering aspects. "Has a type system" and "has evaluation semantics" are facts about participation, not substrate-level switches.

## Procedure identity

Derived entries are produced by procedures (type checkers, evaluators, translators, analyzers). A derived entry is cached by `(definition-hash, aspect, procedure-identity)`. We need a stable way to identify a procedure.

### Near-term — tag + manual version

Procedures are named by a human-assigned tag and version: `stlc:type-check:v1`, `stlc-to-stlc+subtyping:translate:v1`, and so on. When a procedure's behavior changes, the version is bumped by hand. Entries whose `procedure-identity` no longer matches a currently registered procedure are stale and may be recomputed or garbage-collected.

**Honest cost.** This requires discipline. A silent change to a procedure without a version bump causes the cache to return lies. Acceptable during bootstrapping; the alternative (automatic identity via content-addressing) presupposes the meta-language that we aren't building yet.

### Far-term — procedures as content-addressed definitions

Procedures eventually become definitions in some meta-language. Their content hash is their identity. Cache invalidation is automatic and precise. This is the metacircular bootstrap moment — genuinely hard and explicitly deferred.

### Two-way door

The cache key's `procedure-identity` field is opaque from day one. Swapping tag+version for content hashes is an infrastructure change, not a semantic one.

## Kinship with the naming layer

The naming layer (`04-naming-layer.md`) is structurally similar to the aspect store: both are stores of assertions relating strings or values to definition hashes.

- The aspect store keys entries by definition hash, with aspect-tagged values attached: *this definition has this type; this translation; this complexity*.
- The namespace keys entries by name-string, with definition hashes as values: *this name binds to this definition*.

Both systems need **bidirectional lookup.** Beyond "what is this definition's X?", interfaces will want "what definitions have X?" — hash → names for display; type → hashes for type-based search; translation-source → translations; and so on.

Both systems will accrue **per-entry invariants** that differ from each other: at-most-one hash per name in a namespace; possibly at-most-one derived-type entry per procedure identity in the aspect store; many-to-many for translations. Invariants are local to each store's entry shape and will grow in complexity on their own schedules.

For now, the two are separate systems. We recognize the kinship and will revisit unifying their lifecycle machinery when we add branching and history — that's where a shared abstraction would earn its keep. See `decisions.md` for the explicit deferral.

## Implications for languages

- A language is not required to participate in every aspect. Typed languages register for typing aspects; evaluatable languages register for evaluation aspects; a markup or specification language may register only for structural or documentation aspects.
- Translation outputs sit in a translation aspect whose entries reference a new definition in the target language. The "translation produces a new definition with its own hash" model is consistent with "translation is associated data on the source definition": the associated datum *is* the record of the new target definition's existence.
- Evaluation traces, if kept, are derived associated data; whether we keep them is an interface and storage question.

## Non-goals (current phase)

- Procedure content-addressing. Deferred as bootstrap.
- Aspect lifecycle management (deprecation, renaming, migration). We add aspects; we don't yet remove.
- Access control on asserted entries. Single-user model assumed.

## Open sub-questions

Tracked in `open-questions.md` under "Associated data."

- **Vocabulary for "aspect" itself.** Chosen provisionally to avoid TAPL-kind collision. Alternatives if friction appears: *facet*, *annotation*, *attribute*, *channel*.
- **Conflict handling.** When an asserted entry and a derived entry of the same logical property disagree (annotation says `Int→Int`, inference says `Bool→Bool`), the substrate records both. What does it surface? Probably a conflict to the interface, but this needs thought when first encountered.
- **Where aspect descriptors live.** Per-language? Global registry? A mix? Feeds into `08-tech-stack.md`.
- **Cross-aspect dependencies.** Evaluation needs types; translation may need elaboration. Do we formalize these as aspect-graph edges or handle them ad hoc in each procedure?
- **History of asserted entries.** Descriptions and annotations change. Do we keep edit history, and if so at what granularity? Ties into the eventual branching story across both the aspect store and the naming layer.
- **Per-aspect uniqueness invariants.** Each aspect is likely to want its own constraints (at-most-one entry per procedure identity for inferred types; no constraints for translations; etc.). Today we leave this informal; at some point the aspect descriptor will likely need to declare the invariant.
