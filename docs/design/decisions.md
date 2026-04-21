# Decisions

Running log of design commitments. Lightweight ADR: each entry has a date, the decision, a short rationale, and noted alternatives. When a decision is reversed, add a new dated entry rather than editing the old one.

---

## 2026-04-20 — Calls are content-addressed by hash

Stored programs reference other definitions by hash. Names are purely an editing-layer concept; they are not stored in the program.

**Rationale.** Hash-based calling makes the program store self-contained and lineage-stable under renames. It also cleanly separates editing/refactoring concerns from computational semantics.

**Alternatives considered.** Name-based calls resolved at evaluation (Unison actually stores hashes too, so this aligns with Unison); hybrid schemes.

---

## 2026-04-20 — No cross-language references

A definition in language L1 cannot reference a definition in language L2. To reuse an L2 definition from L1, a translation must be run; it produces a new L1 definition with its own hash and a recorded correspondence.

**Rationale.** Keeps each language's portion of the store self-contained. Collapses the cross-language interop problem into the translation problem, which we already need. Avoids having to model a bridging semantics between runtimes.

**Alternatives considered.** Tagged cross-language hashes with FFI-like bridges; a shared core IR with per-language surface views (option C in `01-language-model.md`).

---

## 2026-04-20 — Translations are per-pair and hand-written (for now)

Each directed language pair that supports translation does so via a hand-written translator. The result is a new definition in the target language plus metadata linking source and target.

**Rationale.** Slow and obvious. Avoids committing to a composition model or shared IR before we understand the problem. Noted futures: Cambria-style schema migration, proof-assisted translation, LLM-assisted translation.

**Alternatives considered.** Shared core IR from day one; elaboration-based translation through a common kernel; automated lifting for subset relationships.

---

## 2026-04-20 — Composition model deferred

We are not committing to a single model for how features compose across languages (fixed ladder, feature lattice, shared core IR, hybrid). We implement each language independently and write translations per-pair. We revisit when the friction justifies unification.

**Rationale.** Premature commitment on this axis would constrain every other decision. See `01-language-model.md` for the option space we're holding open.

---

## 2026-04-20 — Effect systems not in scope (current phase)

The languages we add will not track effects in their type systems during the current phase of the project.

**Rationale.** Focus. Effect systems interact badly with many of the features we do plan to add (polymorphism, references, subtyping), and the design space is large.

---

## 2026-04-20 — Strict separation between computational core and interfaces

The core exposes an interface-agnostic API. No interface is privileged. Scratch-buffer-style editing is a likely first interface but not baked into the core.

**Rationale.** Multimodal interaction is a long-term goal. Coupling any interface to the core would foreclose it.

---

## 2026-04-20 — Unified associated-data store; asserted vs. derived distinguished by aspect

Everything attached to a definition — names, type annotations, inferred types, translations, evaluation results, documentation, complexity analyses — lives in a single store tagged by aspect. Whether an aspect is *asserted* (human-authored, mutable) or *derived* (produced by a named procedure, immutable for that procedure's identity) is declared on the aspect itself.

**Rationale.** Avoids a substrate-level static/dynamic typing divide: typing is just aspect participation. Lets markup-style languages (no eval, no types) be first-class citizens. Starting unified preserves the two-way door to later partitioning; starting separate would require migration if we ever wanted to merge.

**Vocabulary note.** We call the category an *aspect* rather than a *kind* to avoid collision with kind-in-type-theory (which will appear literally when we reach F-omega).

**Alternatives considered.** Separate subsystems for asserted metadata and derived artifacts; aspects as per-language schemas rather than a global registry.

---

## 2026-04-20 — Translation is eager over the transitive closure

Running a translator on a source definition translates it and every definition it transitively depends on. Dependencies with existing cached translations (under the same translator identity) are looked up by hash and reused; only uncached dependencies are re-computed. The substrate does not support partial or lazy translation.

**Rationale.** Eager closure is the simplest model that keeps D_L2 self-consistent: by the time the root is translated, every reference it needs in L2 exists. Caching via the aspect store makes repeated closures over overlapping graphs mostly cache hits, so the "eager" cost is paid once per dependency per translator identity.

**Alternatives considered.** Lazy-on-demand (translate only what's needed, resolve forward refs later) — postpones complexity we'd have to solve before the system works end-to-end. Declarative request (translator declares needs; substrate fetches) — extra machinery for no current benefit.

---

## 2026-04-20 — Translators are potential actions; interface exposes choices

The substrate does not select among translators. For each directed `(L1, L2)` pair, the substrate can enumerate the translators registered for it; interfaces are responsible for presenting choices to users or selecting via interface-level policy.

**Rationale.** Multiple legitimate translators for a pair can coexist (different elaboration strategies, inlining policies, faithfulness levels). Baking a selection policy into the substrate would prejudice the interface layer, which we've explicitly kept the substrate independent from.

**Alternatives considered.** A default-translator-per-pair substrate hint; ranking by some metadata. Neither earns its keep yet.

---

## 2026-04-20 — Translator correctness is trusted during bootstrap

We treat hand-written translators as correct by the author's discipline. No substrate-level verification, certification, or property-based testing infrastructure is built in this phase.

**Rationale.** Same posture as procedure identity generally: discipline is the cheap option during bootstrap. Verification is future work and is noted as such in `05-translation.md`.

**Alternatives considered.** Property-based test scaffolding from day one (nice-to-have but premature); certification DSL (heavy); LLM-review loop (out of scope).

---

## 2026-04-20 — Namespace and aspect store are separate systems

The naming layer and the aspect store are structurally related — both hold assertions relating strings or values to definition hashes, both need bidirectional lookup, both will accrue per-entry uniqueness invariants. We keep them as separate subsystems with independent schemas for this phase, rather than introducing a shared "container" abstraction.

**Rationale.** The difference in keying direction (hash-keyed for aspects, name-keyed for names) and the difference in invariants are real. A shared abstraction's real payoff is in sharing branching and history machinery, both of which are deferred. Introducing the abstraction now would be vocabulary without function.

**When to revisit.** When branching enters scope and both subsystems need the same lifecycle infrastructure. The cross-reference sections in `02` and `04` are the pointers we'll use to find this decision again.

**Alternatives considered.** A unified "container" concept (first-class object holding typed entries, shared branching / versioning / history lifecycle) with namespace and aspect store as two instantiations. Rejected for now on overengineering grounds.

---

## 2026-04-21 — Four-layer substrate architecture

The substrate decomposes into four layers with strict upward-only dependencies:

1. **Store** — content-addressed definition storage. Holds typed `Definition.t` values keyed by `Hash.t`. Enforces the "no cross-language references" invariant at registration.
2. **Attachment** — aspect store and namespace(s). Holds typed `AspectValue.t` entries and name → hash bindings. Bidirectional queries.
3. **Language** — per-language modules (AST, canonicalizer, type-checker, evaluator, primitives) and inter-language translators.
4. **Interface** — user-facing modalities.

**Rationale.** Gives each concern a home with a stable API. Keeps interfaces pluggable and languages additive. Makes explicit where cross-cutting things (translators) sit and why. Refines the earlier "core vs. interface" framing, which is now a special case of this.

**Alternatives considered.** Two-way core/interface split (our original framing); collapsing Attachment into Store; making Language a generic plugin infrastructure from day one (premature).

---

## 2026-04-21 — Typed values at substrate API boundaries

All substrate API boundaries deal in structured, typed values rather than byte arrays. Store holds `Definition.t` (a sum over the language enumeration). Attachment holds `AspectValue.t` (a sum over aspect categories with per-language payloads). Namespace entries hold `Hash.t` values. Byte-level serialization, if needed, lives internally within a single layer (e.g., inside the hashing function); it does not cross public APIs.

**Rationale.** Typed boundaries reduce complexity by letting the host language's type system enforce invariants that would otherwise require runtime checks on opaque data. Complexity during bootstrap was the main concern expressed when this was decided.

**Accepted cost.** Adding a language requires extending the `Definition.t` sum (and possibly `AspectValue.t`). Plugin-style languages are future work. Acceptable for single-codebase bootstrap.

**Alternatives considered.** A content-agnostic Store holding opaque bytes — more flexible for eventual plugins, but harder to program against safely.

---

## 2026-04-21 — Prototype strategy: a series of disposable experiments

Implementation work proceeds as **a series of prototype implementations**, not as one long-lived codebase that grows feature-by-feature. Each prototype answers specific design questions and may be discarded when it has served its purpose.

**Rationale.** The design space has too many unknowns to commit to a single implementation trajectory. Cheap, focused prototypes surface real problems faster than incremental evolution of a codebase toward a moving target. Aligns with the project's "step in slowly" posture.

**Where prototype decisions live.** In `docs/prototypes/<prototype-name>/` (created when the first prototype scopes up; does not exist yet). Substrate-level design stays in `docs/design/`. Learnings that refine the substrate design migrate back into `docs/design/` with the prototype cited as source.

**Alternatives considered.** One long-lived implementation walking the TAPL ladder. Rejected because premature commitment to any single trajectory would constrain design choices we want to keep open.

---

## 2026-04-20 — Procedure identity: tag+version now, content-addressed far-future

Derived-aspect entries are cached by `(definition-hash, aspect, procedure-identity)`. Near-term, `procedure-identity` is a human-assigned tag + manually bumped version (e.g., `stlc:type-check:v1`). The long-term goal is for procedures themselves to be content-addressed definitions in a meta-language, at which point their hash *is* their identity.

**Rationale.** Tag+version lets us build the substrate without first building the meta-language. Honest cost: silent changes to procedures without version bumps cause the cache to return stale/incorrect entries — a discipline problem we accept. The cache key keeps `procedure-identity` abstract, so moving to content hashes later is an infrastructure swap, not a semantic one.

**Alternatives considered.** Content-addressed procedures from day one (blocked on designing the meta-language first); ad-hoc per-procedure invalidation (sacrifices the uniform caching model).
