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

## Threads under exploration

The substrate stance — names as opaque strings, structure deferred — leaves several editing-layer questions on the table. The threads below are open: each sharpens what's actually being asked, surfaces the structural connections that matter, and explicitly stops short of resolution. None has yet been exercised by a prototype.

### Hierarchical paths

A common convention for organizing names is the dot-delimited path: `data.list.map`, `examples.numbers.factorial`. The thread is *which layer carries the convention*, not whether one exists. Three options:

- **Editing-layer convention only.** Substrate keeps names as opaque strings; specific interfaces choose `.`, `/`, or any other separator. Prefix queries ("list everything under `data.list.*`") are synthesized by the interface scanning the flat namespace. This is the current implicit stance.
- **Substrate-aware structure, interface rendering.** Substrate exposes paths as `list(string)` (or similar) with prefix-based query primitives. Interfaces pick how to render — `.`, `/`, breadcrumb chips, tree views. Substrate-level operations like prefix-rebind become possible.
- **Substrate-fixed string format.** A single canonical separator at the substrate level; interfaces are expected to follow.

The pivot is what hierarchy is load-bearing for. Display and bulk rename can probably stay in the editing layer over a flat namespace. Branching at sub-namespaces (Unison's "merge `core.math`") plausibly cannot, and would push hierarchy into the substrate. The choice is bound up with when (or whether) namespace branching enters scope.

### Tags

A second naming-shaped concept: tags. The cardinality differs from names. Names are constrained one-per-string within a namespace — alias-many is permitted in the hash → name direction, but a single name binds to a single hash. Tags would be many-to-many: a tag value (e.g. `deprecated`, `tutorial:chapter-3`) can attach to many hashes, and a hash can carry many tags, with no uniqueness constraint either direction.

The connection worth surfacing: this looks like an *asserted aspect* (`02-definitions-and-derived-data.md`). The aspect store already keys typed associated data by `(definition-hash, aspect-id)`, and asserted aspects are exactly the shape of human-authored, mutable attachments. A `tag` aspect with a string-valued payload would land in that machinery; reverse queries (`tag-value → hashes`) are the same shape as type-based search, which the aspect store already accommodates.

Sub-questions if tags fit there:

- **Cardinality on the aspect-store key.** Today's `(hash, aspect-id) → value` keying implies one value per pair. Many-valued aspects (tags or anything similar) need either a list-valued aspect or relaxed key uniqueness. Either way, the choice generalizes beyond tags.
- **Whether tag values are themselves structured.** `domain:crypto`, `chapter:3` — colon-delimited convention only, or a substrate-recognized tag-namespace? Possibly its own thread.
- **Whether anything in tags resists the aspect reading.** Interface UX (autocomplete, tag clouds, faceted search) is presumably layerable on top, but worth checking against a concrete need.

The thread to keep open: is a tag just an asserted aspect, or is there something it needs that aspects don't provide?

### Leaf names independent of path

Suppose one definition (one hash) has two namespace bindings whose final segment matches: `core.math.factorial` and `examples.numbers.factorial`. The intuition is that the leaf name (`factorial`) is what the function is *called*, while the path is where it *lives*. Renaming the leaf in one path could be expected to propagate to the other.

Sharpening what this is and isn't:

- Linking by leaf only makes sense for the same hash. Two different definitions that happen to share a leaf segment are coincidentally homonymous and shouldn't be linked.
- Even for one hash, leaf-linking can be unwanted. A tutorial copy at `examples.factorial` and a production binding at `core.math.factorial` may want to drift — one stays "factorial" for pedagogical reasons while the other becomes `fact`.
- The analogy to content-addressing is partial. Hashes are stable because they derive from content; leaf names are strings, stable only by discipline. Promoting a leaf to a first-class identity is a modeling choice, not a derivation.

Two stances on how to model it:

- **Leaf as first-class entity.** A leaf carries an identity; path bindings attach paths to a leaf for a given hash. Rename-the-leaf updates all attached paths. New schema; the namespace becomes a 3-way relation rather than a 2-way map.
- **Bulk-rename UX over today's flat namespace.** No schema change. The interface, on rename of `a.factorial` → `a.fact` for hash `H`, surfaces other bindings of `H` whose leaf is also `factorial` and offers per-binding confirmation.

The first promotes a new identity into the substrate; the second keeps the substrate flat and treats leaf-coupling as an editing-layer affordance. Either could be wrong and we wouldn't yet know.

### Update strategies

When a name is rebound to a new hash, the no-silent-breakage property from §"Interaction with content addressing" means callers of the old hash are not automatically updated. `open-questions.md` §"Update strategies" lists three natural editor strategies — *pin* (never propagate), *follow* (always track latest), *explicit migration* (user-driven rewrite). The synthesis below is exploratory: it recasts those three as points in a 2-axis space, identifies the substrate primitives a real implementation would touch, and is recorded as a working leaning rather than a settled design. No prototype has exercised the cascade primitives yet.

**Two axes, not three peer strategies.**

- *Scope* — how far updates propagate. None, a chosen subset, or all reachable callers.
- *User-in-loop* — whether the substrate cascades autonomously, or the user intervenes (to resolve breakage, to pick which call sites participate, or both).

The three named strategies fall out as points in that space:

- **Pin.** Scope: none. User-in-loop: no. Rebind the name; *orphan* the old hash — no name resolves to it, but caller content keeps it reachable. Pin reads naturally as the substrate's resting behavior, but it earns the "strategy" name because the user is making an active choice to decline propagation.
- **Follow.** Scope: all reachable callers. User-in-loop: no. The substrate auto-cascades — for each direct caller of the old hash, substitute the new hash, re-canonicalize, re-typecheck, ingest, rebind the caller's name, recurse. Available cleanly only when a dry-run confirms the whole cascade type-checks end-to-end.
- **Explicit migration.** Scope: a chosen subset. User-in-loop: yes. The editor surfaces the call-graph reachable from the old hash; the user toggles each site in or out, and resolves whatever breakage the cascade could not handle on its own.

The actual UX is hybrid rather than binary: follow runs as far as it can, explicit picks up only the sites it could not. The signal that tells the user up front whether intervention will be required at all is itself a derivable aspect — `follow-clean(h_old, h_new): bool` — which can be cached, and which a type-preserving edit (same `Type_of` on both sides, pure substitution) short-circuits without running the cascade.

**Substrate primitives this would touch.**

- *Reverse-DAG query*: hash → direct-caller hashes. Implicit in structural sharing today; would become an explicit query.
- *Atomic multi-rebind*: the cascade commits as a unit so partial states do not leak.
- *`follow-clean` aspect*: per-(h_old, h_new) cached predicate over the dry-run cascade. Short-circuited by type-preserving edits.
- *Binding history*: append-only per name, `list((hash, timestamp))`. Optional annotations (commit-message-shaped) layer as aspects on history entries — not built into the entry itself.

**Orphan display depends on binding history.** Under pin, the old hash carries no current name. Rendering it as a bare hash is hostile; rendering it with its prior name plus a disambiguator (`Math.calc#abc`, `Math.calc(v3)`) is informative. That UX makes binding history load-bearing for the interface — not merely a debugging convenience — even though the substrate-level need for history remains modest.

**Synonyms.** Two names binding the same hash are independent rows in the namespace; the substrate as currently designed has no built-in notion that they are "the same thing." Whether they should track together under rebind depends on whether the substrate carries identity *across edits*, not merely at a moment. The working leaning (recorded in `10-minted-identity.md` §"Marks that survive content edits") is that a mint mark preserved across edits supplies exactly that identity, and that the mint is best read as the user's *signal* that this term has identity worth preserving — intentional, not automatic. Under that reading, the substrate exposes the grouping (which definitions share mint `m`); the editor uses it or ignores it as appropriate, and "update all bindings of mint `m`" becomes a substrate primitive rather than an editor reconstruction.

**Patch.** A unit of co-dependent edits — the "checked-out context" referenced under `open-questions.md` §"Editing context" — is a natural longer-term home for a *patch*: a set of `(old_hash, new_hash, name?)` triples plus per-pair strategy and any explicit transformations. Whether such a patch is itself content-addressed is left to a later prototype. For now it can sit as the editor's working set — promote it only when a real use forces the question.

## Non-goals (current phase)

- Namespace branching, merging, diffing.
- Namespace history / time-travel.
- Multi-user or shared namespaces.
- Automated "update all callers" when a name is rebound. Provided as an editor action, not a substrate guarantee.
- Hash-qualified names as a disambiguation device (Unison's `f#h1a2b3` convention — a name plus a hash prefix, used to pick one of several referents). Our substrate namespace is unambiguous by construction; ambiguity handling can wait. *(Description corrected 2026-08-07. This bullet previously read "suffix-based name disambiguation (Unison's `f.h1a2b3` convention)," which conflated two unrelated mechanisms and contradicted the prototypes. **Hash-qualification** is the non-goal above. **Suffix resolution** — writing `map` for `base.List.map` when the suffix is unambiguous — is a different mechanism, and p9, p10, and p17 all implement it, with a distinct `Ambiguous` error; see `../prototypes/p9-typed-namespaces/00-scope.md`. Unison's version is richer than any of them: three suffixification strategies chosen by context, plus a dependency-depth priority rule under which names nested deeper below `lib` lose ties, so `lib.base.List.map` wins over `lib.something.lib.base.Set.map` for the bare suffix `map`. arbor has no equivalent of that rule and will want one once namespaces carry vendored dependencies. `../systems/unison/03-namespace-and-history.md` §"Suffix resolution".)*

## Open sub-questions

Tracked in `open-questions.md` under "Naming layer."

- **Name structure.** Substrate default is opaque strings. Three sub-threads sharpened in *Threads under exploration* above: hierarchical paths, tags, leaf-names independent of path. Language-qualified prefixes remain a separate un-elaborated thread.
- **Language-qualified names.** If two languages want to use the same logical name (e.g., `factorial`), do we expect the editing layer to scope names per language, or to present a cross-language search that disambiguates by language?
- **Rename as an explicit operation.** Is rename atomic at the substrate level (one op) or derived (unbind + bind)? Matters for future history tracking.
- **Handling dangling references on display.** When a caller references a hash that has no current name, what's the default interface behavior? Probably surface the hash with a "prior version" marker, but this is UX.
- **When multiple namespaces arrive.** What's the data model — separate objects, forking, a namespace graph? Deferred until branching is actually on the table.
