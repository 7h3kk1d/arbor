# Label Sort

**Status:** Draft. Exploration — no prototype has exercised records or labels yet.

## Purpose

The substrate has two sorts of definition today: `Term` and `Type` (introduced in p9). The natural next compound, records — and modules, which this doc treats as the same shape with a different idiomatic use — introduces a third concept the substrate has to take a position on: the identity of a field inside a record.

The naive position is "a field is identified by its label string." A record type becomes something like `Record [("x", Int); ("y", Int)]`, with the strings sitting inside the canonical bytes. That position is the moral equivalent of putting term names back inside stored programs: it re-couples the human-readable label to the record's hash. Rename `x` to `position_x` in any one place and every record-type that mentioned `x` gets a new hash; every term whose type referenced one of those records gets a new hash; the namespace-rename UX the substrate worked to enable for top-level definitions stops at the record boundary.

The position this doc opens a thread on is to promote labels to a first-class *sort* of definition — alongside `Term` and `Type` — content-addressed, namespace-bound by human-readable name, and *minted by default* per `10-minted-identity.md`. Records reference label hashes rather than label strings. Renaming a label is a namespace operation; record-type hashes do not move. Coincidental name reuse across contexts produces distinct labels, so two `x` fields declared in unrelated places never quietly conflate.

## The core idea

- **`Definition.t` gains a third constructor.** `Term | Type | Label`. Labels join terms and types as the substrate's stored sorts. A label has a hash, can be bound to a human-readable name in the namespace, and can be cross-referenced from other definitions exactly like a term or type can.
- **Labels are minted by default.** Declaring a label draws a fresh mint mark (per `10-minted-identity.md`). Two declarations producing the same human-readable name mint distinct labels with distinct hashes. This is the obvious posture for labels: the whole point of treating labels as their own sort is to keep semantically-different "x"s apart.
- **The namespace binds names to label hashes.** Exactly the existing name → hash binding shape from `04-naming-layer.md`. A name like `geom.point.x` resolves to a label hash. Multiple names can bind the same label hash (aliasing), and a label can be renamed without anything else moving.
- **Records reference labels by hash.** A record type encodes as `Record [(label_hash_1, ty_hash_1); (label_hash_2, ty_hash_2); …]`. The record's canonical bytes contain label hashes and type hashes; no label name ever appears. Two record types are hash-equal iff they pair the same label hashes with the same type hashes.
- **Renaming a label is free at the type level.** The label hash is stable across rename. Every record type that referenced the label is structurally and content-addressedly unchanged. The naming layer absorbs the rename; the Store doesn't see it.

## Where labels enter

Following the surface→internal→node discipline used elsewhere:

- **Surface.** The language exposes a declaration gesture that mints a label. Concrete syntax is the language's choice — a `label` keyword, a dedicated declaration form, or implicit first-use in a record literal (a thread, below). The gesture's substrate-level meaning is fixed: mint a fresh label, return its hash to the namespace.
- **Internal AST.** Records carry `(label_hash, ty)` pairs instead of `(string, ty)`. Field projection is `Project(record_expr, label_hash)` rather than `Project(record_expr, "x")`. Record literals carry `(label_hash, value_expr)` lists.
- **Node encoding.** Label hashes participate in the record's canonical bytes; label names never do. A `Label` node's own canonical encoding is the mint mark plus whatever the language wants to declare about the label (a "see also" type, a documentation string aspect — but those are aspects, not content).
- **Pretty-printing.** The renderer reverse-looks-up label hashes through the namespace and prints human-readable names. When no name is bound, fall back to a short-hash display, paralleling p7/p9's behavior for unbound term hashes.

A label, like a minted term, has no "body" the system can hash over the way it can for terms and types. Its canonical encoding is essentially `tag-byte + mint-mark`. This is the same shape as a primitive's manually-versioned tag (`03-content-addressing.md:38–50`); labels are the user-facing version of the same maneuver.

## Records and modules: same shape, different idiom

A *module* in this design is a record whose field types are reference-typed — the field values are references to other definitions in the Store. A *record* is the general shape; a *module* is the convention of using one for "a collection of named bindings." The substrate does not need a separate sort for modules; one label sort serves both populations.

Worked examples:

- **Record.** `type Point = { x: Int, y: Int }`. The labels `x` and `y` are minted; `Point` references their hashes alongside `Int`. The namespace binds `geom.point.x` and `geom.point.y` to the label hashes.
- **Module.** `module math = { add: Term[Int → Int → Int], mul: Term[Int → Int → Int] }`. The labels `add` and `mul` are minted; the field values are term references. The "shape" is the same; the reading is "this is a collection of bindings."

Treating them as one mechanism means the rename UX, the structural-sharing wins, and the minted-by-default discipline come along uniformly. Whether the language layer wants to surface `record` and `module` as separate keywords or as one is its own call.

## Threads under exploration

### First-use semantics, and where minting lives

The declaration-vs-use distinction is sharp for `label x` syntax, but record literals like `{ x = 1, y = 2 }` use a label without obviously declaring one. The substrate's preferred reading is the third one: **record literals resolve label names through the namespace exactly like any other name**, and minting is always an explicit gesture. This keeps the model legible (no invisible minting events embedded in literal syntax) and aligns label use with how terms and types already work.

The ergonomics problem — users don't want to declare every label upfront before writing a record literal — is pushed up into the *editing layer* rather than answered at the substrate. The proposed shape:

- **The editor offers auto-mint affordances.** When the user types `{ x = 1 }` and `x` is unresolved, the editor surfaces a quick option to mint a fresh label and bind it to `x`, rather than silently doing so. The substrate sees a normal "mint + bind" gesture; the user sees a one-keystroke convenience.
- **Pre-commit marking.** Before committing a draft, the editor offers a pass where the user can mark which referenced names should be treated as fresh mints versus resolved to existing bindings. This separates the "type the literal" gesture from the "decide what's new" gesture, and keeps minting visible.

The substrate's part of this contract is small: a clean namespace-resolution semantics for label references, and an unambiguous "mint and bind to this name" primitive that the editor can call. The flexibility lives at the editing layer where it belongs.

### Field order in the canonical form

`03-content-addressing.md:25` already names "Normalizing record or variant field order, if the language's semantics treat them as unordered" as a per-language canonicalization choice. With label-hashes available, the substrate's preferred default is **canonicalize by sorted label hash**. This makes records hash-permutation-invariant and gives a stable, name-independent ordering — the canonical form does not depend on which human-readable names happen to be bound, so renames don't perturb it. The ordering is determined entirely by the label *hashes*, which are themselves stable.

Declaration order remains defensible for languages that treat records as ordered (FFI, serialization stories), and the per-language opt-out from `03` still applies. But absent a reason to preserve declaration order, sort-by-label-hash is the default that flows most naturally from the rest of the design.

### Variants, constructors, method names

The label-sort generalizes beyond record fields, and we want it to. Variant tags (`Some`, `None`, `Left`, `Right`), constructor names in data declarations, and method names in module-as-interface idioms all share the same identity shape: an opaque, name-decoupled handle whose human-readable rendering lives in the namespace. The substrate's preferred direction is **one label sort serving all of these populations**.

What that buys: renaming a constructor is a namespace operation; coincidental constructor-name collisions across unrelated sum types stay distinct by minting; pattern-match clauses reference constructor labels by hash, so a renamed `Some` doesn't perturb any matcher's stored form. The price is that the label sort has to carry enough structure (a "see also" type, payload expectations) for variant uses without forcing record-field uses to deal with it. Probably an aspect on the label rather than a richer label-node shape — keeps the sort flat while letting variants attach what they need.

The harder distinction — whether some typing disciplines eventually need to *separate* record-field labels from sum-constructor labels at the type-language level — is left open. The substrate-level move is the same either way.

### Intentional label sharing across types

Two record types both declaring a field named `x` mint distinct labels by default. That is the right default — it preserves the property that coincidental name reuse doesn't conflate semantically-different fields. But sometimes users want the opposite: `Point` and `Vector` both have an `x` field that *is the same notion*, and a function quantifying over "things with an x-coordinate" should accept both.

The substrate-level move is straightforward: a user declares a label once (minting it) and uses the resulting label hash in both `Point` and `Vector`. Both record types now reference the same label hash; downstream tooling can recognize the sharing structurally. The editing layer's job is making this gesture pleasant — a "use existing label" affordance that shows when the user types `x` in a new record context and an `x` label is already in scope.

This is the substrate-level shape of the **row-polymorphism / module-subtyping** question. We expect to need one or both eventually:

- Row-typing over records, where a function quantifies over "records with at least these fields" and field identity is a label hash.
- Subtyping over modules, where one module is a subtype of another when it provides at least the labeled bindings the other requires.

Either feature consumes intentional label sharing — without shared label hashes, "the same x" has no substrate-level meaning. The label sort thus has to support both *fresh by default* (so unrelated `x`s don't conflate) and *explicitly shared on demand* (so structural subtyping has something to bite on). The two are not in tension; they're the same mechanism used two ways. Left open: what the editor affordance for "use existing label" looks like, and whether the type-language needs explicit row variables or whether structural subtyping over fixed label sets is enough for early prototypes.

## Relation to existing concepts

- **Minted identity** (`10-minted-identity.md`). Labels are the first concrete population that *requires* minting to behave correctly. The general mechanism in 10 specializes here: "minted by default" rather than the per-definition opt-in posture 10 left open for definitions broadly. This doc thus partly answers 10's per-definition-vs-per-language thread by example — for the label sort, mint always.
- **Named types in p9** (`docs/prototypes/p9-typed-namespaces/00-scope.md`). p9 added `Definition.t = Term | Type` and let the namespace bind names to type hashes. Labels are the same maneuver a third time. The pattern — promoting some previously-inline concept to a content-addressed sort with namespace bindings — is becoming a substrate idiom worth naming on its own.
- **Naming layer** (`04-naming-layer.md`). Labels reuse the existing name → hash binding shape; no namespace-schema change. The phrasing in `04-naming-layer.md:115` — "promoting a leaf to first-class identity is a modeling choice, not a derivation" — applies almost verbatim to labels.
- **Field-order canonicalization** (`03-content-addressing.md:25`). The label-hash availability changes what canonicalizations are even expressible; the per-language choice from 03 still stands but gains a more attractive option (sort by label hash).
- **Hashing types as well as terms** (`03-content-addressing.md:80`). Labels strengthen the case. Once labels live in the store, record types that reference them are mostly small hash-permutations of each other; structural deduplication starts paying off where it didn't for monomorphic scalar types.
- **Procedure identity** (`02-definitions-and-derived-data.md:67`). A label's canonical encoding (tag + mint mark) is structurally the same as a primitive's canonical encoding (tag + version + signature). Labels are user-facing minted entities; primitives are substrate-facing tagged entities; the family resemblance is intentional.

## Non-goals (current phase)

- **No prototype prescription.** The natural target is the future TAPL-Ch. 11 prototype where records first appear. The thread can mature in design docs first.
- **Row polymorphism and module subtyping are destinations, not present scope.** The label-sort design explicitly leaves room for them (see §"Intentional label sharing across types"), but the type-language work to support them is deferred.
- **No variant/constructor implementation commitment.** This doc commits to the *direction* (one label sort serving variants too) but not to a specific encoding; that's for the prototype that introduces sums.

## Open sub-questions

Tracked in `open-questions.md` under "Label sort."

- **Editor affordances for minting and pre-commit marking.** The substrate's semantics are namespace-resolution + explicit mint-and-bind; the editing-layer UX that makes this pleasant (auto-mint shortcut, pre-commit label-marking pass, "use existing label" suggestion) is open.
- **How variants attach payload-type information to a label.** Aspect on the label vs. a richer label-node shape vs. a separate sort, eventually.
- **Recursive modules.** If a module's fields refer to other definitions in the same module, the substrate needs a story for content-addressing the mutual reference. This intersects the existing "Mutual recursion canonicalization" thread in `open-questions.md` §"Content addressing" and probably gets resolved there once a language forces the issue.
- **Editor affordance for label sharing.** Once two record types intentionally share a label, the editor needs to surface "use this existing label" suggestions; what triggers them, how they're presented, and how the user disambiguates is open.
- **Type-language shape that consumes shared labels.** Whether early prototypes get away with structural subtyping over fixed label sets, or need explicit row variables from the start.

## Connections

- `10-minted-identity.md` — labels are the first concrete sort minted by default; partial answer-by-example to 10's per-definition vs. per-language opt-in thread.
- `03-content-addressing.md:25` — the existing "Normalizing record or variant field order" mention is the touch point for field-order canonicalization with label-hashes.
- `03-content-addressing.md:80` — "Hashing types as well as terms"; labels intensify the deduplication argument for record types.
- `04-naming-layer.md` — labels reuse the name → hash binding shape unchanged.
- `02-definitions-and-derived-data.md:67` — labels and primitives share the "opaque-identity-baked-into-hash" maneuver.
- `01-language-model.md:86` — TAPL Ch. 11 is the planned home for records; the natural prototype trigger.
- `docs/prototypes/p9-typed-namespaces/00-scope.md` — p9's promotion of types to a sort is the immediate precedent.
