# The Naming Layer

**Status:** Draft.

## Purpose

Establish the substrate's name → hash mapping system: how humans work with definitions via identifiers rather than hashes, and how that stays compatible with the content-addressed storage beneath. This doc covers only the substrate mechanism. Higher-level editing UX (scratch buffers, refactoring commands, diff views) belongs to `06-architecture.md` (Interface layer).

## Scope and posture

Single-user, single-namespace for this phase. The naming layer is structured so that multi-namespace, branching, merging, and history (Unison-style) can be added later without substrate-level refactoring. We are not paying for any of those capabilities now; we are just not foreclosing them.

## Core concept: the namespace

A **namespace** is a first-class object holding name → hash bindings.

- For the current phase there is one namespace. This is not hardcoded as a singleton; the substrate API treats namespaces as objects from the start, so introducing multiple later is additive.
- A namespace entry is a pair `(name, hash)`. The language of the referenced definition is a property of the definition itself (per `03-content-addressing.md`) and is not part of the namespace entry.
- Multiple names can point to the same hash (aliases are free).
- A single name cannot bind to multiple hashes in one namespace. Resolution is deterministic; no ambiguity at this layer.

## Names

- Names are **opaque strings** at the substrate level.
- The substrate does not impose hierarchy, separator conventions, or reserved prefixes. Any structure — dot-paths, slash-paths, tags, language-qualified prefixes — is an editing-layer convention that can vary by interface.
- This preserves the path to Unison-style hierarchical namespaces later without committing now.

## Resolution

- **At edit time.** Name → hash resolution happens when an editor produces a stored program. Stored programs carry only `Ref(hash)`; names are gone by the time anything is persisted.
- Resolution is a pure function: given a namespace and a name, return a hash or a "not found" result.
- At display / load time, interfaces may do **reverse lookup** (hash → names) to show human-friendly identifiers. This is also a namespace query.
- When reverse lookup finds no name for a referenced hash (common after editing), the interface decides how to present the bare hash or an "outdated" marker. The substrate stays out of this.

## Mutation

Namespaces are mutable. Operations:

- **Bind** — assign a name to a hash.
- **Rebind** — change which hash a name points to.
- **Unbind** — remove a name.
- **Rename** — unbind one name, bind another to the same hash.

History of these mutations is not tracked in this phase. Branching eventually requires history; deferred.

## Interaction with content addressing

The naming layer **never mutates stored programs.** When a name is rebound:

- Existing `Ref(hash)` entries in other definitions are unaffected.
- Callers remain pinned to their authoring-time hashes.
- Upgrading a caller to use the new target is an explicit edit action: re-resolve the name, rebuild the caller's AST with the new hash, produce a new hash for the caller, rebind that caller's name to point to the new hash.

This is the "no silent breakage" property from `03-content-addressing.md`. "Upgrade all callers" is a bulk operation the editing layer may offer; the substrate performs no automatic rewriting.

## Relationship to the aspect store

Names live in the namespace, not in the definition-level aspect store from `02-definitions-and-derived-data.md`. The distinction:

- **Aspects** are properties *of* a definition — they follow the definition's identity. Keyed by `(definition-hash, aspect-id)`.
- **Names** are bindings *to* a definition, organized by namespace — they follow the namespace's identity. Keyed by `name-string`.

Multiple namespaces (future) produce multiple independent name-hash mappings without duplicating the underlying definitions or their aspects. That would be awkward if names were aspects.

Despite living in separate subsystems, the namespace and the aspect store have **real structural kinship**. Both are stores of assertions linking strings or values to definition hashes; both need bidirectional lookup (name → hash *and* hash → name; aspect-value → hashes *and* hash → aspect-value, as in type-based search); both will accrue per-entry uniqueness invariants that differ by store and aspect. We keep them separate today because the difference in keying direction and invariants is real, and because a unified abstraction would mostly earn its keep around branching and history — which we've deferred. When branching arrives we'll reconsider.

Reverse queries on either system are derivable by scanning; indexing may be added for performance without changing the model.

## Future: branching

Unison's central UX contribution is namespace branching and merging: you work in your branch, then merge your changes. We explicitly defer this, but model namespaces as first-class objects so that:

- Multiple namespaces can coexist without schema changes.
- Namespace history becomes meaningful when we add branching.
- Merge operations can be added as namespace-to-namespace operations.

Nothing in the current design forecloses this path; nothing in the current design pays for it either.

## Non-goals (current phase)

- Namespace branching, merging, diffing.
- Namespace history / time-travel.
- Multi-user or shared namespaces.
- Automated "update all callers" when a name is rebound. Provided as an editor action, not a substrate guarantee.
- Suffix-based name disambiguation (Unison's `f.h1a2b3` convention). Our namespace is unambiguous by construction; ambiguity handling can wait.

## Open sub-questions

Tracked in `open-questions.md` under "Naming layer."

- **Name structure.** Substrate default is opaque strings. Do we impose any conventions from the editing-layer side — hierarchical paths à la Unison, flat identifiers, tags, language-qualified prefixes?
- **Language-qualified names.** If two languages want to use the same logical name (e.g., `factorial`), do we expect the editing layer to scope names per language, or to present a cross-language search that disambiguates by language?
- **Rename as an explicit operation.** Is rename atomic at the substrate level (one op) or derived (unbind + bind)? Matters for future history tracking.
- **Handling dangling references on display.** When a caller references a hash that has no current name, what's the default interface behavior? Probably surface the hash with a "prior version" marker, but this is UX.
- **When multiple namespaces arrive.** What's the data model — separate objects, forking, a namespace graph? Deferred until branching is actually on the table.
