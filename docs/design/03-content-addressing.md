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

- Normalizing record or variant field order, if the language's semantics treat them as unordered. (See `11-label-sort.md` for the substrate's preferred treatment of field labels — once labels are content-addressed hashes rather than strings, sort-by-label-hash becomes a natural canonical order.)
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

## Threads under exploration

### Hashing types as well as terms

So far only term-language definitions are content-addressed. Types appear inline in term encodings: an STLC `Lam(ty, body_hash)` writes `ty`'s structural bytes into the parent's hash input (p6, p9), and the `stlc:type-check:v1` aspect value `Type_of(Ty.t)` is an inline OCaml sum. p6 *does* hash types — but only to encode them compactly into procedure-identity suffixes (`lc-to-stlc:check:v1[ty=<hex8>]`), under a separate `'T'` tag so type hashes don't collide with definition hashes. The hash is a string-encoding tactic; types are not stored as definitions. Prototype seeds: `docs/prototypes/p6-stlc/open-questions.md` §"Hashing Ty" and §"Ty hash space"; `prototype-findings.md` §"Raised once" → "Hashed type space for non-Hash aspect values."

The thread is whether to promote types to first-class content-addressed objects. What changes concretely if so:

- **Term encodings carry type references.** `Lam(ty_hash, body_hash)` instead of inlining `ty`'s bytes. Type identity factors out of every term that mentions it; structural type equality becomes hash equality.
- **Types live in the store.** A type definition is a definition in some type-language; it has a hash; canonicalization for types (field order, type-variable α-equivalence once binders show up) is the type-language's job, paralleling term canonicalization.
- **Aspect values reference types by hash.** `Type_of(hash)` instead of `Type_of(Ty.t)`. The aspect-value sum stops carrying inline type ASTs; the type itself is reachable by hash.
- **Names can bind to type hashes.** This is the type-aliasing connection (below) and follows for free from `04-naming-layer.md`'s shape.

#### The type-aliasing reading

Pascal/Haskell-style type aliases are usually a separate language feature — `type` declarations, transparent expansion, scope rules of their own. If types are content-addressed *and* the namespace already binds names to hashes, an alias is just multiple names binding the same type hash, with the same semantics names already have for terms. `type Vector = Int -> Int -> Int` becomes a namespace bind; two names pointing at the same type hash are aliases by content, not by lookup-chasing.

Newtypes (distinct identity for an isomorphic carrier) remain a separate concept: they require the type-language to introduce a constructor that produces a *different* hash from its inner type. Alias vs. newtype thus becomes a type-language design choice, not a substrate one — the substrate doesn't need to know which it is. The substrate-level generalization of the newtype maneuver — opt-in opaque identity stamped into the canonical form for any definition, not just types — is sharpened in `10-minted-identity.md` as *minted identity*.

#### What's open

- **One global hash-space, or two?** "There is one global hash-space" (above) was stated for term-language definitions. Extending it to types is natural but unstated. If types share the space, every type definition needs a language tag — either a single meta-type-language, or per-target-language type-languages with disjoint tags. If types live in a separate space, the substrate gains a second store-shaped object and "one global hash-space" needs softening.
- **Cross-language type identity.** If STLC and a future STLC+subtyping both use `Int → Int`, do they share a type hash, or do their type-languages have disjoint tags? Sharing is appealing for translation (same type, same hash, same reverse-query results); not sharing keeps language identity rigid in the way `decisions.md` requires for terms. Probably bound up with the composition-model question in `01-language-model.md`.
- **When does it pay off?** For monomorphic primitives (`Int`, `Bool`, small `Arrow`s) inline embedding is small and the deduplication win is small. The case strengthens with polymorphic types (∀, ∃ — TAPL Ch. 23–24), recursive types (Ch. 20), and large structural records — types stop being trivially small, reverse queries ("which definitions have type T?") start mattering for type-based search, and the editing-layer story for naming a type gets worth telling.
- **F-omega lurking.** Once types are first-class objects with their own canonical form, the substrate is closer to F-omega's regime where types have their own type system (kinds — the word reserved for that). Adopting type-hashing too early may pre-shape decisions better deferred to when F-omega is being instantiated.
- **Migration cost.** Moving from inline `Type_of(Ty.t)` to `Type_of(hash)` is a one-time aspect-value-shape change, not a re-architecture; the discipline question is when the cost of inline embedding shows up in a prototype.

#### Connections

- `04-naming-layer.md` — names binding type hashes is the alias mechanism; no namespace schema change needed if types share the term hash-space.
- `02-definitions-and-derived-data.md` — `Type_of(...)` aspect-value shape is the touch point.
- `01-language-model.md` — content-addressed types nudge gently toward Option C (shared core IR) by giving types a canonical form independent of any specific term language.

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
- **Hashing types as well as terms.** Whether to promote types to first-class content-addressed objects — making type aliasing a free consequence of the namespace and changing how type-valued aspects are shaped. Sharpened above under *Threads under exploration*.
- **Minted identity.** Whether definitions can opt into an opaque, non-stringly-typed identity stamped into their canonical form, so coincidentally-equal structural twins do not deduplicate and the substrate has a stable handle for editing history and update propagation. Sharpened in `10-minted-identity.md`.
