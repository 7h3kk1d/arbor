# Content Addressing

**Status:** Draft.

## Purpose

Pin down how definitions are identified, how they reference each other, and how their hashes are grounded at the leaves. This doc covers only the hashing substrate. Caching of derived associated data is in `02-definitions-and-derived-data.md`; translation-specific caching is in `05-translation.md`.

## Identity

A definition is identified by its hash alone. We treat cryptographic hash collisions as impossible in practice.

- **Hash → definition** is a globally unique lookup. Given a hash, we return the full definition, which carries its language as a property.
- **Language is associated data on the definition,** not a component of identity. (This is a modeling convenience; it does not weaken the "no cross-language references" rule — that rule is enforced as a construction-time invariant, below.)
- There is one global hash-space. We do not partition by language.

## Hash input: canonicalized AST

The hash is over a *canonicalized* form of the definition's AST. Each language declares its canonicalizer. At minimum every language should collapse **α-equivalence** so that `λx.x` and `λy.y` hash identically.

Canonicalization is **syntactic by default.** Experimenting with coarser equivalences — β- or η-normalization, normal-form-up-to-reduction, extensional equality — is a per-language opt-in we haven't exercised. The door is intentionally open: a language may declare a more aggressive canonicalizer if that's the experiment it wants to run. Note that coarser equivalences discard distinctions that matter for resource behavior and for the programmer's intent, so the choice is not free.

Other per-language canonicalizations a language might declare:

- Normalizing record or variant field order, if the language's semantics treat them as unordered.
- Canonicalizing the presentation order of mutually recursive members (when we add recursion).
- Normalizing type-variable names in languages with explicit type binders.

## References in stored programs

Every cross-definition reference inside a stored program is a hash reference — `Ref(hash)` — never a name.

- **Names never appear in stored programs.** Name resolution happens at edit time and produces a hash reference; the stored artifact carries only the hash.
- **No cross-language references.** Per `decisions.md`, a definition in language L1 cannot carry a reference to a definition in language L2. The substrate enforces this as a construction-time invariant: when a definition is registered, every `Ref(hash)` in its body must resolve to a definition whose language matches the referring language. References that would span languages are rejected. Reuse across languages flows through translation.
- **Callers pin automatically.** Editing a definition produces a new hash; existing callers still reference the old hash. There is no silent migration. Upgrading callers is the editing layer's job — not the substrate's. See `04-naming-layer.md`.
- **Hashes ground transitively.** Definition A's hash depends on the hashes of definitions A references, which depend on theirs, and so on, bottoming out at primitives.

## Primitive identity

Primitives (language builtins with no body the system can hash over) ground the whole hash chain.

Following the procedure-identity decision from `02-definitions-and-derived-data.md`, primitives use the same model: **manually-versioned tags.**

- A primitive is identified by `(namespaced-tag, version, signature)`, e.g., `int:add:v1` with a declared signature in its language.
- This triple is what a definition's canonical AST embeds at a primitive use site. The hash therefore includes the triple verbatim.
- Changing a primitive's behavior requires bumping its version. Every definition transitively depending on the primitive then gets a new hash.

**Honest cost.** Same discipline problem as procedure identity: a silent change to a primitive's native implementation without a version bump produces hashes that lie. Acceptable during bootstrapping; full state resets are an accepted fallback in this phase.

**Far-term alignment.** The metacircular goal from `02-definitions-and-derived-data.md` extends here: primitive behaviors could eventually be expressed in a meta-language and content-addressed, except for genuinely native primitives (FFI, machine arithmetic) where we'll likely still need a tag pointing to a native implementation. Deferred.

## Unison as prior art — what we adopt and what we don't

**Adopt (in spirit):**

- Per-definition content addressing with α-equivalent canonical forms.
- Hash-based references in stored programs; names in a separate layer.
- The property that hashes never dangle: a definition that exists always has all its dependencies present.
- Hashing mutually recursive groups as a single canonical unit (long-term; see open sub-questions).

**Do not adopt (current phase):**

- **Automatic migration UX (`update`, `patch`, etc.).** When a definition changes, Unison helps users systematically move callers to the new hash. We have no equivalent. Callers remain pinned until an editing action explicitly rebinds them.
- **Shared codebases and distribution.** Unison's networked code-sharing model is out of scope (non-goal in `00-overview.md`).
- **Their specific codebase representation** (base-32 textual hashes, wire format, `.u` file convention). Implementation choices we'll make independently when we get to the tech stack.

The "no silent breakage" property is what we get from the substrate alone: a call resolves to exactly the definition it referenced at authoring time, or the reference is dead in a visible way. Bulk-update and refactoring UX is additive, handled by the naming layer and interfaces.

## Hash representation

Specifics (algorithm choice, encoding, length) are not locked in. Constraints:

- Deterministic across implementations.
- Collision-resistant enough that accidental collisions are impossible in practice.

Anything else is deferred. During bootstrap, if we need to change the representation, we accept full state rebuilds. We are not designing migration machinery for this phase.

## Non-goals (current phase)

- **Migration machinery.** Automatically moving callers when a definition changes.
- **Distribution, shared codebases, networked publishing.**
- **Locking in a hash algorithm.**
- **Hashing partial / ill-formed programs.** The long-term direction here is different from how it first sounds: hole-aware languages (from the Hazel lineage) will encode incompleteness as first-class AST constructs, so a "program with holes" in a hole-aware language is not partial — it's a well-formed program in a richer language, hashable and canonicalizable like any other. We don't need generic "partial program" machinery in the substrate; we just need each hole-aware language to declare its own canonical form when it's introduced. Deferred until we add one.

## Open sub-questions

Tracked in `open-questions.md` under "Content addressing."

- **Mutual recursion canonicalization.** When a language introduces mutually recursive definitions, we'll need a canonical ordering for the group so the hash is stable. Unison's approach is a known starting point; not urgent until a language introduces recursion.
- **Holes and incomplete programs.** If the substrate eventually hashes incomplete programs (Hazel-style editing workflows), how do holes participate in the canonical form? Unique hole identities, wildcards that make hash matching a subsumption relation, or something else?
- **Cross-version primitive aliasing.** If `int:add:v1` and `int:add:v2` differ only cosmetically, callers of v1 are orphaned. Is there an aliasing story, or is this just accepted cost of the manual-version discipline?
