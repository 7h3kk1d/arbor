# Decisions

Running log of design commitments. Each entry has a date, the decision, a short rationale, and the alternatives considered. A reversal gets a new dated entry; old entries are not edited.

---

## 2026-04-20 — Calls are content-addressed by hash

Stored programs reference other definitions by hash. Names belong to the editing layer and are not stored in the program.

**Rationale.** Hash-based calls make the program store self-contained and stable under renames. They also separate editing and refactoring concerns from computational semantics.

**Alternatives considered.** Name-based calls resolved at evaluation (Unison also stores hashes, so this choice aligns with Unison); hybrid schemes.

---

## 2026-04-20 — No cross-language references

A definition in language L1 cannot reference a definition in language L2. To reuse an L2 definition from L1, run a translation. It produces a new L1 definition with its own hash and a recorded correspondence.

**Rationale.** Each language's portion of the store stays self-contained. Cross-language interop reduces to translation, which we need anyway. No bridging semantics between runtimes has to be modelled.

**Alternatives considered.** Tagged cross-language hashes with FFI-like bridges; a shared core IR with per-language surface views (option C in `01-language-model.md`).

---

## 2026-04-20 — Translations are per-pair and hand-written (for now)

Each directed language pair that supports translation has a hand-written translator. Its result is a new definition in the target language plus metadata linking source and target.

**Rationale.** Slow and obvious. It avoids committing to a composition model or shared IR before we understand the problem. Possible futures: Cambria-style schema migration, proof-assisted translation, LLM-assisted translation.

**Alternatives considered.** Shared core IR from day one; elaboration-based translation through a common kernel; automated lifting for subset relationships.

---

## 2026-04-20 — Composition model deferred

We do not commit to a model for how features compose across languages (fixed ladder, feature lattice, shared core IR, hybrid). Each language is implemented independently, with per-pair translations. We revisit when the friction justifies unification.

**Rationale.** Committing early on this axis would constrain every other decision. `01-language-model.md` describes the option space.

---

## 2026-04-20 — Effect systems not in scope (current phase)

The languages added in the current phase do not track effects in their type systems.

**Rationale.** Focus. Effect systems interact badly with several planned features (polymorphism, references, subtyping), and the design space is large.

---

## 2026-04-20 — Strict separation between computational core and interfaces

The core exposes an interface-agnostic API, and no interface is privileged. A scratch buffer is a likely first interface but is not built into the core.

**Rationale.** Multimodal interaction is a long-term goal, and coupling any interface to the core would rule it out.

---

## 2026-04-20 — Unified associated-data store; asserted vs. derived distinguished by aspect

Everything attached to a definition lives in one store tagged by aspect: names, type annotations, inferred types, translations, evaluation results, documentation, complexity analyses. Each aspect declares whether it is *asserted* (human-authored, mutable) or *derived* (produced by a named procedure, immutable for that procedure's identity).

**Rationale.** There is no substrate-level static/dynamic typing divide; typing is participation in an aspect. Markup-style languages with no evaluation and no types are first-class. Starting unified keeps later partitioning possible, while starting separate would need a migration to merge.

**Vocabulary note.** The category is called an *aspect* to avoid colliding with kinds in type theory, which will appear literally at F-omega.

**Alternatives considered.** Separate subsystems for asserted metadata and derived artifacts; aspects as per-language schemas instead of a global registry.

---

## 2026-04-20 — Translation is eager over the transitive closure

Running a translator on a definition translates it and every definition it transitively depends on. Dependencies that already have a cached translation under the same translator identity are looked up by hash and reused. Only uncached dependencies are computed. The substrate does not support partial or lazy translation.

**Rationale.** Eager closure is the simplest model that keeps D_L2 self-consistent: once the root is translated, every reference it needs in L2 exists. Caching through the aspect store makes repeated closures over overlapping graphs mostly cache hits, so the cost is paid once per dependency per translator identity.

**Alternatives considered.** Lazy on-demand translation (translate only what is needed, resolve forward references later), which postpones complexity that must be solved before the system works end to end. Declarative requests (the translator declares needs and the substrate fetches them), which is extra machinery with no current benefit.

---

## 2026-04-20 — Translators are potential actions; interface exposes choices

The substrate does not choose among translators. For each directed `(L1, L2)` pair it can enumerate the registered translators. Interfaces present the choices to users or select by interface-level policy.

**Rationale.** Several legitimate translators for one pair can coexist, differing in elaboration strategy, inlining policy or faithfulness. A selection policy in the substrate would prejudge the interface layer, from which the substrate is kept independent.

**Alternatives considered.** A default-translator-per-pair hint in the substrate; ranking by metadata. Neither is needed yet.

---

## 2026-04-20 — Translator correctness is trusted during bootstrap

Hand-written translators are treated as correct on the author's discipline. This phase builds no substrate-level verification, certification or property-based testing infrastructure.

**Rationale.** The same posture as procedure identity: discipline is the cheap option during bootstrap. Verification is future work, noted in `05-translation.md`.

**Alternatives considered.** Property-based test scaffolding from day one (useful but premature); a certification DSL (heavy); an LLM review loop (out of scope).

---

## 2026-04-20 — Namespace and aspect store are separate systems

The naming layer and the aspect store are structurally related. Both hold assertions relating strings or values to definition hashes, both need bidirectional lookup, and both will accrue per-entry uniqueness invariants. For this phase they stay separate subsystems with independent schemas, with no shared "container" abstraction.

**Rationale.** They differ in keying direction (aspects are hash-keyed, names are name-keyed) and in invariants. A shared abstraction would pay off by sharing branching and history machinery, and both are deferred. Introduced now, it would be vocabulary without function.

**When to revisit.** When branching enters scope and both subsystems need the same lifecycle infrastructure. The cross-reference sections in `02` and `04` point back to this decision.

**Alternatives considered.** A unified "container" (a first-class object holding typed entries, with a shared branching, versioning and history lifecycle) with the namespace and aspect store as two instances. Rejected for now as overengineering.

---

## 2026-04-21 — Four-layer substrate architecture

The substrate decomposes into four layers with upward-only dependencies:

1. **Store** — content-addressed definition storage. Holds typed `Definition.t` values keyed by `Hash.t`. Enforces the "no cross-language references" invariant at registration.
2. **Attachment** — aspect store and namespace(s). Holds typed `AspectValue.t` entries and name → hash bindings. Bidirectional queries.
3. **Language** — per-language modules (AST, canonicalizer, type-checker, evaluator, primitives) and inter-language translators.
4. **Interface** — user-facing modalities.

**Rationale.** Each concern gets a home with a stable API. Interfaces stay pluggable and languages additive. It also states where cross-cutting pieces such as translators sit, and why. The earlier core/interface framing becomes a special case.

**Alternatives considered.** A two-way core/interface split (the original framing); folding Attachment into Store; a generic plugin infrastructure for Language from day one (premature).

---

## 2026-04-21 — Typed values at substrate API boundaries

Substrate API boundaries pass structured, typed values, not byte arrays. Store holds `Definition.t`, a sum over the language enumeration. Attachment holds `AspectValue.t`, a sum over aspect categories with per-language payloads. Namespace entries hold `Hash.t` values. Byte-level serialization, where needed, stays inside one layer (for example the hashing function) and does not cross public APIs.

**Rationale.** The host language's type system enforces invariants that would otherwise need runtime checks on opaque data. Bootstrap complexity was the main concern when this was decided.

**Accepted cost.** Adding a language means extending the `Definition.t` sum, and possibly `AspectValue.t`. Plugin languages are future work. This is acceptable for a single-codebase bootstrap.

**Alternatives considered.** A content-agnostic Store holding opaque bytes: more flexible for eventual plugins, harder to program against safely.

---

## 2026-04-21 — Prototype strategy: a series of disposable experiments

Implementation proceeds as a series of prototypes, not one long-lived codebase grown feature by feature. Each prototype answers specific design questions and may be discarded once it has done so.

**Rationale.** The design space has too many unknowns to commit to one implementation trajectory. Cheap, focused prototypes surface real problems faster than evolving a codebase toward a moving target. This fits the project's "step in slowly" posture.

**Where prototype decisions live.** In `docs/prototypes/<prototype-name>/`, created when the first prototype is scoped (it does not exist yet). Substrate-level design stays in `docs/design/`. Learnings that refine the substrate design migrate back into `docs/design/`, citing the prototype.

**Alternatives considered.** One long-lived implementation walking the TAPL ladder. Rejected because committing to one trajectory would constrain choices we want to keep open.

---

## 2026-04-23 — Project name: **arbor**

The project is named arbor (Latin for *tree*). The working directory stays `lc-content-addressed` for now. The name is for the design, the eventual prospectus, and any artifacts separate from the scratch repo.

**Rationale.** It evokes the content-addressed DAG of definitions and the trees the aspect layer grows over it: namespaces, type derivations, translation lineages. It is short, pronounceable and ASCII-light, and does not leak an implementation detail (such as "lc-" or "content-addressed") that might not survive design changes. The Latin root parallels the "substrate" framing without overloading a term already used in the docs.

**Scope of the rename.** The name is introduced where it reads naturally: the overview, this entry, the open-questions closeout. It is not a global search-and-replace. The working directory, prototype directory names and in-code identifiers keep their current form until there is a reason to change them.

**Alternatives considered.** Keeping `lc-content-addressed`, which reveals the project's origins but becomes inaccurate once the arithmetic/λ-calculus scaffolding is retired. No name until a prospectus demands one, which delays a hook for the research-framing question (still open).

---

## 2026-04-20 — Procedure identity: tag+version now, content-addressed far-future

Derived-aspect entries are cached by `(definition-hash, aspect, procedure-identity)`. Near term, `procedure-identity` is a human-assigned tag plus a manually bumped version (for example `stlc:type-check:v1`). Long term, procedures should be content-addressed definitions in a meta-language, and their hash becomes their identity.

**Rationale.** Tag+version lets us build the substrate before the meta-language. The cost: changing a procedure without bumping its version makes the cache return stale or incorrect entries, a discipline problem we accept. The cache key keeps `procedure-identity` abstract, so moving to content hashes later swaps infrastructure without changing semantics.

**Alternatives considered.** Content-addressed procedures from day one (blocked on designing the meta-language); ad-hoc per-procedure invalidation (gives up the uniform caching model).
