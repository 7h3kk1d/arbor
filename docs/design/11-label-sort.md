# Label Sort

**Status:** Draft. Written before any prototype had records or labels; p16 and p17 have since implemented labels as a minted sort (see their scope docs).

## Purpose

p9 gave the substrate two sorts of definition, `Term` and `Type`. Records, and modules, which this doc treats as the same shape used differently, raise a third question: what identifies a field inside a record?

The naive answer is the label string. A record type becomes `Record [("x", Int); ("y", Int)]`, with the strings in the canonical bytes. That puts names back inside stored programs and re-couples the label to the record's hash. Renaming `x` to `position_x` gives every record type that mentioned `x` a new hash, and every term whose type referenced one of them a new hash too. The namespace-rename UX that works for top-level definitions stops at the record boundary.

This doc proposes promoting labels to a first-class sort beside `Term` and `Type`: content-addressed, bound to human-readable names in the namespace, and minted by default per `10-minted-identity.md`. Records reference label hashes instead of strings, so renaming a label is a namespace operation and record-type hashes do not move. Coincidental name reuse produces distinct labels, so two `x` fields declared in unrelated places do not conflate.

## The core idea

- **`Definition.t` gains a third constructor**, `Term | Type | Label`. A label has a hash, can be bound to a name, and can be referenced from other definitions like a term or type.
- **Labels are minted by default.** Declaring a label draws a fresh mint mark (`10-minted-identity.md`), so two declarations of the same name produce distinct labels with distinct hashes. Keeping semantically different `x`s apart is the reason for the sort.
- **The namespace binds names to label hashes**, using the name → hash shape from `04-naming-layer.md`. `geom.point.x` resolves to a label hash. Several names can bind one label (aliasing), and a label can be renamed without anything else moving.
- **Records reference labels by hash.** A record type encodes as `Record [(label_hash_1, ty_hash_1); (label_hash_2, ty_hash_2); …]`, with no label name in its bytes. Two record types are hash-equal iff they pair the same label hashes with the same type hashes.
- **Renaming a label leaves types unchanged.** The label hash is stable across rename, so every record type that referenced it is unchanged. The naming layer absorbs the rename and the Store does not see it.

## Where labels enter

Surface → internal → node:

- **Surface.** The language exposes a gesture that mints a label: a `label` keyword, a dedicated declaration form, or implicit first use in a record literal (thread below). Its substrate meaning is fixed: mint a fresh label and return its hash to the namespace.
- **Internal AST.** Records carry `(label_hash, ty)` pairs instead of `(string, ty)`. Projection is `Project(record_expr, label_hash)`. Record literals carry `(label_hash, value_expr)` lists.
- **Node encoding.** Label hashes enter the record's canonical bytes; label names never do. A `Label` node encodes as its mint mark plus anything the language declares as content about the label. A "see also" type or a documentation string would be aspects, not content.
- **Pretty-printing.** The renderer reverse-looks-up label hashes in the namespace. An unbound label falls back to a short-hash display, as p7/p9 do for unbound term hashes.

A label has no body to hash over, so its canonical encoding is essentially `tag-byte + mint-mark`. That is the shape of a primitive's manually versioned tag (`03-content-addressing.md:57–67`); labels are the user-facing version of the same maneuver.

## Records and modules: same shape, different idiom

A module here is a record whose field values are references to other definitions in the Store. The record is the general shape and the module is the convention of using one as a collection of named bindings, so one label sort serves both.

- **Record.** `type Point = { x: Int, y: Int }`. Labels `x` and `y` are minted; `Point` references their hashes alongside `Int`. The namespace binds `geom.point.x` and `geom.point.y` to them.
- **Module.** `module math = { add: Term[Int → Int → Int], mul: Term[Int → Int → Int] }`. Labels `add` and `mul` are minted and the field values are term references.

With one mechanism, the rename UX, structural sharing and minted-by-default apply uniformly. Whether a language surfaces `record` and `module` as separate keywords is its own choice.

## Threads under exploration

### First-use semantics, and where minting lives

A `label x` declaration clearly mints, but a literal like `{ x = 1, y = 2 }` uses labels without obviously declaring them. The preferred reading is that record literals resolve label names through the namespace like any other name, and minting is always an explicit gesture. No minting event hides inside literal syntax, and label use works as term and type use already does.

The ergonomic cost, declaring every label before writing a literal, is handled in the editing layer:

- **Auto-mint affordance.** When the user types `{ x = 1 }` and `x` is unresolved, the editor offers a one-keystroke option to mint a label and bind it to `x`, without doing so silently. The substrate sees an ordinary mint-and-bind.
- **Pre-commit marking.** Before committing a draft, the editor offers a pass in which the user marks which referenced names are fresh mints and which resolve to existing bindings. This separates typing the literal from deciding what is new, and keeps minting visible.

The substrate owes two things: namespace-resolution semantics for label references, and an unambiguous mint-and-bind primitive for the editor to call.

### Field order in the canonical form

`03-content-addressing.md:44` lists "Normalizing record or variant field order, if the language's semantics treat them as unordered" as a per-language canonicalization choice. With label hashes available, the preferred default is to **canonicalize by sorted label hash**. Records become permutation-invariant, and the order depends only on the label hashes, which are stable, not on which names are bound, so renames do not perturb it.

Declaration order remains defensible for languages that treat records as ordered (FFI, serialization), and the per-language opt-out from `03` still applies. Absent such a reason, sort by label hash.

### Variants, constructors, method names

Variant tags (`Some`, `None`, `Left`, `Right`), constructor names in data declarations, and method names in module-as-interface idioms have the same identity shape as record fields: an opaque handle whose rendering lives in the namespace. The preferred direction is one label sort for all of them.

Renaming a constructor then becomes a namespace operation, coincidental constructor-name collisions across unrelated sum types stay distinct, and pattern-match clauses reference constructor labels by hash, so renaming `Some` does not change any matcher's stored form. The cost is that variant uses need extra structure (a "see also" type, payload expectations) that record fields should not have to carry. An aspect on the label is the likely answer, keeping the sort flat.

Whether some typing disciplines eventually need to separate record-field labels from constructor labels in the type language is open. The substrate move is the same either way.

### Intentional label sharing across types

Two record types that each declare a field `x` mint distinct labels by default, which keeps coincidental name reuse from conflating fields. Sometimes the user wants the opposite: `Point` and `Vector` both have an `x` that is the same notion, and a function over "things with an x-coordinate" should accept both.

At the substrate level the user declares the label once and uses its hash in both `Point` and `Vector`. Tooling can then recognize the sharing structurally. The editor's job is a "use existing label" affordance that appears when the user types `x` in a new record context and an `x` label is in scope.

This is the substrate shape of the **row-polymorphism / module-subtyping** question. One or both will eventually be needed:

- Row typing over records, where a function quantifies over records with at least certain fields and field identity is a label hash.
- Subtyping over modules, where a module is a subtype of another if it provides at least the labeled bindings the other requires.

Both depend on intentional label sharing, since without shared label hashes "the same x" has no substrate meaning. The label sort supports fresh-by-default and explicitly-shared-on-demand with one mechanism. Open: what the "use existing label" affordance looks like, and whether early prototypes need explicit row variables or can manage with structural subtyping over fixed label sets.

## Relation to existing concepts

- **Minted identity** (`10-minted-identity.md`). Labels are the first population that requires minting to behave correctly. They take "minted by default" where 10 left per-definition opt-in open, which partly answers 10's per-definition vs. per-language thread by example: for the label sort, always mint.
- **Named types in p9** (`docs/prototypes/p9-typed-namespaces/00-scope.md`). p9 added `Definition.t = Term | Type` and namespace bindings to type hashes. Labels repeat the move: promote a previously inline concept to a content-addressed sort with namespace bindings. This is becoming a substrate idiom.
- **Naming layer** (`04-naming-layer.md`). Labels reuse the name → hash binding shape with no schema change. `04-naming-layer.md:125`, "promoting a leaf to first-class identity is a modeling choice, not a derivation", applies to labels almost verbatim.
- **Field-order canonicalization** (`03-content-addressing.md:44`). Label hashes make a new canonicalization expressible; the per-language choice from 03 stands, with sort-by-label-hash as an added option.
- **Hashing types as well as terms** (`03-content-addressing.md:103`). Labels strengthen the case. Record types that reference stored labels are mostly small permutations of shared hashes, so deduplication pays off where it did not for monomorphic scalar types.
- **Procedure identity** (`02-definitions-and-derived-data.md:60`). A label's encoding (tag + mint mark) has the same structure as a primitive's (tag + version + signature). Labels are user-facing minted entities and primitives are substrate-facing tagged ones.

## Non-goals (current phase)

- **No prototype prescription.** The expected target was the TAPL Ch. 11 prototype where records first appear (this turned out to be p16).
- **Row polymorphism and module subtyping are deferred.** The design leaves room for them (§"Intentional label sharing across types"); the type-language work is not in scope.
- **No variant/constructor encoding.** The direction (one label sort serving variants too) is set; the encoding belongs to the prototype that introduces sums.

## Open sub-questions

Tracked in `open-questions.md` under "Label sort."

- **Editor affordances for minting, pre-commit marking and label sharing.** The substrate semantics are namespace resolution plus explicit mint-and-bind. The editing-layer UX is open: the auto-mint shortcut, the pre-commit marking pass, and the "use existing label" suggestion, including what triggers it, how it is presented, and how the user disambiguates.
- **How variants attach payload-type information to a label.** An aspect on the label, a richer label node, or eventually a separate sort.
- **Recursive modules.** A module whose fields refer to each other needs a content-addressing story for the mutual reference. This meets the "Mutual recursion canonicalization" thread in `open-questions.md` §"Content addressing" and will probably be resolved there once a language forces it.
- **Type-language shape that consumes shared labels.** Structural subtyping over fixed label sets, or explicit row variables from the start.

## Connections

- `10-minted-identity.md` — labels are the first sort minted by default.
- `03-content-addressing.md:44` — field-order normalization, the touch point for sorting by label hash.
- `03-content-addressing.md:103` — "Hashing types as well as terms"; labels strengthen the deduplication argument.
- `04-naming-layer.md` — the name → hash binding shape, reused unchanged.
- `02-definitions-and-derived-data.md:60` — labels and primitives share opaque identity baked into the hash.
- `01-language-model.md:86` — TAPL Ch. 11, the planned home for records.
- `docs/prototypes/p9-typed-namespaces/00-scope.md` — p9's promotion of types to a sort, the immediate precedent.
