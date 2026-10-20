# The Naming Layer

**Status:** Draft.

## Purpose

The substrate's name → hash mapping: how people work with definitions through identifiers instead of hashes, compatibly with the content-addressed storage beneath. This doc covers only the substrate mechanism. Editing UX (scratch buffers, refactoring commands, diff views) belongs to the Interface layer in `06-architecture.md`.

## Scope and posture

Single user, single namespace for this phase. The layer is structured so that multiple namespaces, branching, merging, and history (Unison-style) can be added later without substrate refactoring. None of those is paid for now, and none is foreclosed.

## Core concept: the namespace

A **namespace** is a first-class object holding name → hash bindings.

- There is one namespace in this phase, but the API treats namespaces as objects from the start, so adding more is additive.
- An entry is a pair `(name, hash)`. The referenced definition's language is a property of the definition (per `03-content-addressing.md`), not of the entry.
- Several names can point to one hash, so aliases are free.
- One name cannot bind several hashes in one namespace. Resolution is deterministic, with no ambiguity at this layer.

## Names

- Names are **opaque strings** at the substrate level.
- The substrate imposes no hierarchy, separator convention, or reserved prefix. Dot-paths, slash-paths, tags, and language-qualified prefixes are editing-layer conventions that may vary by interface.
- This keeps Unison-style hierarchical namespaces reachable without committing to them.

## Resolution

- **At edit time.** Name → hash resolution happens when an editor produces a stored program. Stored programs carry only `Ref(hash)`; names are gone before anything is persisted.
- Resolution is a pure function from a namespace and a name to a hash or "not found".
- At display or load time, interfaces may do **reverse lookup** (hash → names) to show readable identifiers. This is also a namespace query.
- When reverse lookup finds no name for a referenced hash, which is common after editing, the interface decides how to show the bare hash or an "outdated" marker. The substrate stays out of it.

## Mutation

Namespaces are mutable. Operations:

- **Bind**: assign a name to a hash.
- **Rebind**: change which hash a name points to.
- **Unbind**: remove a name.
- **Rename**: unbind one name and bind another to the same hash.

Mutation history is not tracked in this phase. Branching will eventually require it.

## Interaction with content addressing

The naming layer **never mutates stored programs.** When a name is rebound:

- Existing `Ref(hash)` entries in other definitions are unaffected.
- Callers stay pinned to their authoring-time hashes.
- Upgrading a caller to the new target is an explicit edit: re-resolve the name, rebuild the caller's AST with the new hash, produce the caller's new hash, and rebind the caller's name to it.

This is the "no silent breakage" property from `03-content-addressing.md`. The editing layer may offer "upgrade all callers" as a bulk operation; the substrate performs no automatic rewriting.

## Relationship to the aspect store

Names live in the namespace, not in the aspect store of `02-definitions-and-derived-data.md`.

- **Aspects** are properties *of* a definition and follow its identity. Keyed by `(definition-hash, aspect-id)`.
- **Names** are bindings *to* a definition and follow the namespace's identity. Keyed by name.

Future multiple namespaces will give independent name → hash mappings without duplicating definitions or their aspects, which would be awkward if names were aspects.

The two stores are structurally kin: both relate strings or values to hashes, both need bidirectional lookup, and both will accrue store-specific uniqueness invariants. `02-definitions-and-derived-data.md` §"Kinship with the naming layer" sets this out, along with the deferral of any shared abstraction until branching and history arrive. Reverse queries on either store are derivable by scanning; indexes may be added for performance without changing the model.

## Future: branching

Unison's central UX contribution is namespace branching and merging: work in a branch, then merge. This is deferred, but namespaces are first-class objects so that:

- multiple namespaces can coexist without schema changes;
- namespace history becomes meaningful once branching exists;
- merge can be added as a namespace-to-namespace operation.

### One constraint on that operation, established rather than assumed

`formalism/paper/mini-unison.tex` §7 models Unison's merge, and `prototypes/p21-unison-model/src/merge.re` runs it. Two results bear on the last bullet above.

**Merge is namespace-to-namespace, and that part is cheap.** Two stores grown independently from a common ancestor union without reconciliation, because the store is append-only and keyed by content. The model shares one store across all three namespaces and contains no store-reconciliation code, since there is nothing for it to do. This is also most of what immutability buys at merge time. Everything difficult happens in the namespace, which is not content-addressed.

**A merge whose diff is keyed by name inherits a rename problem.** Unison's diff is a map from names, and its test for "did a human write this?" is whether the definition renders differently, which is name-dependent by construction. That makes propagated updates invisible, as intended, and makes renames indistinguishable from authorship. Three failure modes follow, all reproduced:

1. A rename plus an edit manufactures a conflict in a definition nobody opened.
2. A rename alone produces no conflict, but the merge still fails, because the file is rendered under one branch's names and parsed against a namespace holding the other's.
3. A rename raced by an edit merges cleanly and silently undoes the rename, because a delete loses to an update and a rename is a delete plus an add.

So if arbor takes up merge, the obvious construction, a name-keyed diff, is the one known to fail. The alternatives are to key the diff by **referent**, which arbor can do and Unison cannot cheaply, since arbor's namespace is a partial function and its reverse index already exists; or to pass the **rename relation** to the diff explicitly. Unison computes that relation (`Merge/Rename.hs`) but, at the pinned commit, passes it only to a debug printer. It also groups by syntactic hash, so it finds only renames that carry no edit, which is the case that does not conflict. Neither alternative has been designed here.

## Threads under exploration

With names as opaque strings and structure deferred, several editing-layer questions stay open. Each thread below sharpens the question and its connections and stops short of resolving it. None has been exercised by a prototype.

### Hierarchical paths

Dot-delimited paths are the common convention: `data.list.map`, `examples.numbers.factorial`. The question is which layer carries the convention. Three options:

- **Editing-layer convention only.** The substrate keeps opaque strings, and interfaces choose `.`, `/`, or another separator. Prefix queries ("everything under `data.list.*`") are synthesized by scanning the flat namespace. This is the current implicit stance.
- **Substrate-aware structure, interface rendering.** The substrate exposes paths as `list(string)` or similar, with prefix-query primitives, and interfaces choose the rendering: `.`, `/`, breadcrumb chips, tree views. Substrate operations such as prefix-rebind become possible.
- **Substrate-fixed string format.** One canonical separator at the substrate level, which interfaces follow.

The deciding question is what hierarchy is load-bearing for. Display and bulk rename can probably stay in the editing layer over a flat namespace. Branching at sub-namespaces (Unison's "merge `core.math`") probably cannot, and would push hierarchy into the substrate. The choice is tied to when, or whether, namespace branching enters scope.

### Tags

Tags are naming-shaped but have different cardinality. A name binds one hash within a namespace, though a hash may have many names. Tags would be many-to-many: a tag value (e.g., `deprecated`, `tutorial:chapter-3`) can attach to many hashes, and a hash can carry many tags, with no uniqueness either way.

That looks like an *asserted aspect* (`02-definitions-and-derived-data.md`). The aspect store already keys typed associated data by `(definition-hash, aspect-id)`, and asserted aspects have the shape of human-authored, mutable attachments. A `tag` aspect with a string payload would fit, and reverse queries (`tag-value → hashes`) have the same shape as type-based search.

Sub-questions if tags fit there:

- **Cardinality on the aspect-store key.** `(hash, aspect-id) → value` implies one value per pair. Many-valued aspects need either list-valued aspects or relaxed key uniqueness, a choice that generalizes beyond tags.
- **Structured tag values.** Is `domain:crypto` or `chapter:3` a colon convention only, or a substrate-recognized tag namespace? Possibly its own thread.
- **Resistance to the aspect reading.** Interface UX (autocomplete, tag clouds, faceted search) presumably layers on top, but should be checked against a concrete need.

The open question: is a tag just an asserted aspect, or does it need something aspects do not provide?

### Leaf names independent of path

Suppose one hash has two bindings with the same final segment: `core.math.factorial` and `examples.numbers.factorial`. The intuition is that the leaf (`factorial`) is what the function is called and the path is where it lives, so renaming the leaf in one place might propagate to the other.

What this is and is not:

- Linking by leaf only makes sense for the same hash. Different definitions that share a leaf are coincidental homonyms and should not be linked.
- Even for one hash, linking can be unwanted. A tutorial copy at `examples.factorial` and a production binding at `core.math.factorial` may drift, one staying "factorial" for teaching while the other becomes `fact`.
- The analogy to content addressing is partial. Hashes are stable because they derive from content; leaf names are strings, stable only by discipline. Making a leaf a first-class identity is a modeling choice, not a derivation.

Two ways to model it:

- **Leaf as first-class entity.** A leaf has an identity, and path bindings attach paths to a leaf for a given hash. Renaming the leaf updates every attached path. This is a new schema: the namespace becomes a three-way relation instead of a two-way map.
- **Bulk-rename UX over today's flat namespace.** No schema change. On renaming `a.factorial` → `a.fact` for hash `H`, the interface lists other bindings of `H` whose leaf is `factorial` and asks per binding.

The first adds an identity to the substrate; the second keeps the substrate flat and treats leaf coupling as an editing affordance. There is not yet evidence for either.

### Update strategies

When a name is rebound, the no-silent-breakage property (§"Interaction with content addressing") means callers of the old hash are not updated. `open-questions.md` §"Update strategies" lists three editor strategies: *pin* (never propagate), *follow* (always track the latest), and *explicit migration* (user-driven rewrite). The synthesis below recasts them as points in a two-axis space and identifies the substrate primitives an implementation would touch. It is a working leaning, not a settled design, and no prototype has exercised the cascade primitives yet.

**Two axes.**

- *Scope*: how far updates propagate. None, a chosen subset, or all reachable callers.
- *User-in-loop*: whether the substrate cascades autonomously or the user intervenes, to resolve breakage, to choose call sites, or both.

The three strategies are points in that space:

- **Pin.** Scope none, no user in the loop. Rebind the name and *orphan* the old hash: no name resolves to it, but callers' content keeps it reachable. Pin is the substrate's resting behavior; it counts as a strategy because the user actively declines propagation.
- **Follow.** Scope all reachable callers, no user in the loop. The substrate cascades: for each direct caller of the old hash, substitute the new hash, re-canonicalize, re-typecheck, ingest, rebind the caller's name, and recurse. Available cleanly only when a dry run confirms the whole cascade type-checks.
- **Explicit migration.** Scope a chosen subset, user in the loop. The editor shows the call graph reachable from the old hash; the user toggles each site in or out and resolves whatever breakage the cascade could not handle.

In practice the UX is hybrid: follow runs as far as it can, and explicit migration picks up the sites it could not. Whether intervention will be needed at all is itself a derivable, cacheable aspect, `follow-clean(h_old, h_new): bool`. A type-preserving edit (same `Type_of` on both sides, pure substitution) short-circuits it without running the cascade.

**Substrate primitives this would touch.**

- *Reverse-DAG query*: hash → direct-caller hashes. Implicit in structural sharing today; would become an explicit query.
- *Atomic multi-rebind*: the cascade commits as a unit so partial states do not leak.
- *`follow-clean` aspect*: a cached predicate per `(h_old, h_new)` over the dry-run cascade, short-circuited by type-preserving edits.
- *Binding history*: append-only per name, `list((hash, timestamp))`. Optional commit-message-like annotations layer on as aspects of history entries, not as fields of the entry.

**Orphan display depends on binding history.** Under pin, the old hash has no current name. Showing a bare hash is hostile; showing its prior name with a disambiguator (`Math.calc#abc`, `Math.calc(v3)`) is informative. That makes binding history load-bearing for the interface, even though the substrate's own need for history stays modest.

**Synonyms.** Two names bound to one hash are independent namespace rows, and the substrate has no notion that they are the same thing. Whether they should move together on rebind depends on whether the substrate carries identity across edits. The working leaning (`10-minted-identity.md` §"Marks that survive content edits") is that a mint mark preserved across edits supplies that identity, and that the mint is best read as the user's intentional signal that a term has identity worth preserving. The substrate then exposes the grouping (which definitions share mint `m`), the editor uses or ignores it, and "update all bindings of mint `m`" becomes a substrate primitive instead of an editor reconstruction.

**Patch.** A unit of co-dependent edits, the "checked-out context" of `open-questions.md` §"Editing context", is a natural longer-term home for a *patch*: a set of `(old_hash, new_hash, name?)` triples plus a strategy per pair and any explicit transformations. Whether a patch is itself content-addressed is left to a later prototype. For now it can be the editor's working set, promoted only when a real use forces the question.

## Non-goals (current phase)

- Namespace branching, merging, diffing.
- Namespace history and time travel.
- Multi-user or shared namespaces.
- Automatic "update all callers" on rebind. Offered as an editor action, not a substrate guarantee.
- Hash-qualified names as a disambiguation device (Unison's `f#h1a2b3`: a name plus a hash prefix, used to pick one of several referents). The substrate namespace is unambiguous by construction, so ambiguity handling can wait.

Until 2026-08-07 the last bullet read "suffix-based name disambiguation (Unison's `f.h1a2b3` convention)", which conflated two mechanisms and contradicted the prototypes. **Suffix resolution**, writing `map` for `base.List.map` when the suffix is unambiguous, is a different mechanism, and p9, p10, and p17 all implement it with a distinct `Ambiguous` error (see `../prototypes/p9-typed-namespaces/00-scope.md`). Unison's version is richer: three suffixification strategies chosen by context, plus a dependency-depth priority rule under which names nested deeper below `lib` lose ties, so `lib.base.List.map` beats `lib.something.lib.base.Set.map` for the bare suffix `map`. arbor has no such rule and will want one once namespaces carry vendored dependencies. `../systems/unison/03-namespace-and-history.md` §"Suffix resolution".

## Open sub-questions

Tracked in `open-questions.md` under "Naming layer."

- **Name structure.** The substrate default is opaque strings. Three sub-threads are sharpened under *Threads under exploration*: hierarchical paths, tags, and leaf names independent of path. Language-qualified prefixes are a separate, unelaborated thread.
- **Language-qualified names.** If two languages want the same logical name (e.g., `factorial`), should the editing layer scope names per language, or present a cross-language search that disambiguates by language?
- **Rename as an explicit operation.** Is rename atomic at the substrate level, or derived (unbind + bind)? This matters for future history tracking.
- **Dangling references on display.** When a caller references a hash with no current name, what should interfaces do by default? Probably show the hash with a "prior version" marker, but this is UX.
- **Multiple namespaces.** Separate objects, forking, a namespace graph? Deferred until branching is on the table.
