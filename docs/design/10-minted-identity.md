# Minted Identity

**Status:** Draft. Written before any prototype exercised minting; p10 (mint-everything), p11 (mint threads), p12 (`Opaque` types) and p16/p17 (labels) have since exercised parts of it, and the p12 finding is recorded below.

## Purpose

The substrate identifies every definition by its content hash alone. α-equivalence canonicalization (`03-content-addressing.md` §"Hash input") amplifies this: two definitions that differ only in bound-variable names share one hash, one Store entry and one set of aspect rows.

Sometimes that collapse is wrong. Two definitions can be structurally identical by coincidence and still mean different things to their authors. Unison's example is `unique type Meters` vs. `unique type Feet`, both `Int`-shaped. The same pattern appears for terms (two zero-argument constants both equal to `0`, two empty record values), for tags and enum cases (two payload-free singleton constructors), and wherever identity needs to survive content edits so that editing history and update propagation can attach to it.

This doc explores *minted identity*: an opt-in axis of identity, composable with content addressing and baked into the canonical form, so that minted definitions do not deduplicate against coincidentally-equal neighbours.

## The core idea

A created definition may be *minted*. At creation the substrate stamps it with a *mint mark*, an opaque value drawn fresh from a generator the user does not control. The mark is part of the canonical AST fed to the hash function, so two definitions that differ only in their marks get different hashes.

Unminted definitions stay *structural* and behave as they do today. Minting is a per-definition property, not a global mode.

Properties to preserve:

- **The mark is opaque.** It is not a string or a name, and the user never types it. The human-readable name lives in the naming layer as it does for structural definitions, so minting does not reintroduce the name-identity coupling that `04-naming-layer.md` removed.
- **The hash remains primary.** A mark adds no second lookup key; it only changes which definitions collapse. `Hash → definition` stays globally unique, and every cross-definition reference in a stored program is still a hash reference (`03-content-addressing.md` §"References in stored programs").
- **Uses look like structural uses.** A reference to a minted definition is `Ref(hash)`. The mark belongs to the referenced definition's content, not to the call site.

### The mark is a nonce

The mint mark is a **nonce**: a value used once and mixed into the hash preimage to force distinct digests, as with the salt in a salted hash commitment. That framing settles three questions.

- **Minting cannot weaken the store.** A commitment scheme's *binding* property is what the content hash already provides, and adding a nonce to the preimage does not affect it. Minting changes only which definitions share an equivalence class. No-dangle, immutability and the primacy of `Hash → definition` follow from binding and are untouched.
- **The mark can be public.** Cryptography adds a nonce to hide a low-entropy value, which requires the salt to be secret and high-entropy. We add it only for distinctness, which requires nothing but uniqueness. The mark can therefore be rendered in a UI, logged and compared. For the same reason the **mint-as-keypair** escalation in `open-questions.md` §"Type abstraction" is a change of category: it adds a secrecy requirement that distinctness never had.
- **Uniqueness is the only requirement on the generator.** This narrows the *How marks are minted* sub-question below to a counter (certainly unique, needs coordinated state) or randomness (unique with overwhelming probability, needs no coordination). Distributed and multi-agent settings conventionally choose randomness because it has no coordination point, which also makes marks non-reproducible under re-ingest by construction.

### The coincidental-convergence hazard

Meters-vs-Feet is coincidence at authoring. A sharper case appears once definitions are edited over time, and it is the one that most clearly requires the mark to be in the hash.

Two distinct concepts `A` and `B` are each edited and both arrive at structurally identical content `C`. Under content addressing alone the two histories collapse at `C`: there is one `C`, and whether it descends from `A` or `B` is unrecoverable. After a further edit, `C ⇒ A'` and `C ⇒ B'` cannot be attributed. The weld is silent. A fresh mark per authoring prevents it, because `C` authored from `A` and `C` authored from `B` are different definitions.

This case is satisfied by the *dies-with-hash* reading. Freshness alone breaks the collapse, and the thread `A → C → A'` is then carried by the namespace binding or an edit log, not by the mark. It therefore supports the p12 position below; marks-survive would only add grouping by mark without consulting the log. What the case does establish is that the mark must be inside the hash, since the hazard is precisely that two definitions would otherwise be one Store entry.

Git answers the lineage half of this hazard more cheaply, with no nonce: a commit's hash includes its parents, so `A ⇒ C` and `B ⇒ C` give two commits sharing one deduplicated tree. Under minting, `C_a` and `C_b` are two definitions and are type-checked and evaluated twice; under a predecessor-carrying version node they would share one set of aspects. That cost is real and is why mint-everything is not free. A parent pointer does not cover the distinctness role, though. It lives in a separate layer, so `Ref(hash-of-C)` cannot tell which concept it denotes, whereas a mark in the hash makes every reference carry the distinction. Prior art and the fuller comparison (the ABA problem, surrogate vs. natural keys and Type 2 slowly-changing dimensions, why CRDTs cannot content-address their elements) are in `../related-work/01-content-addressing.md` §"Coincidental convergence".

A nonce buys distinctness and nothing else. It says two definitions are not the same; it does not say they are versions of one thing. The marks-survive thread below is therefore a separate question from how marks are generated. Reusing a nonce is what the construction forbids, so marks-survive is a knowing misuse of the mechanism.

## Where the mint mark enters

Surface → internal → node:

- **Surface.** A creation gesture in the editing layer signals that a definition is minted: a language keyword like Unison's `unique`, a substrate-level flag at ingest, or a per-language always-mint declaration (threads below).
- **Internal AST.** The definition carries a `Mint(mark)` field or constructor beside its body. Structural definitions carry none.
- **Node encoding.** The canonical bytes include the mark for minted definitions; structural definitions encode and hash as before. A minted definition hashes differently from its structural twin and from every minted definition with a different mark.
- **Re-ingestion.** Round-tripping a minted definition through serialization preserves its mark. The mark is content, not metadata.

The mint mark is not an aspect: aspects sit outside the hash, and the mark is inside it. A mapping from mark to a richer record (author, creation time, history) could live in the aspect store in the usual way.

## Threads under exploration

### Per-definition vs. per-language opt-in

Two granularities, neither chosen:

- **Per-definition** (Unison-style). `unique X = …` mints and plain `X = …` does not. Fine-grained and user-controlled, but every language that supports minting must surface the gesture.
- **Per-language.** A language's canonicalizer declares that all its definitions are minted, or none. This parallels the per-language canonicalization choice in `03-content-addressing.md` §"Hash input", where α-equivalence collapsing is already a per-language decision. Simpler, but every author in language L shares one regime.

The two could coexist, with per-language setting a default and per-definition overriding it. The choice probably depends on whether minting is a language-design feature (Meters-vs-Feet) or substrate infrastructure (every definition gets a stable identity thread for history); the two push in opposite directions. A third option, per-sort, is described under Unison's `unique` below.

### Marks that survive content edits

When a user edits a minted definition, the edit produces a new hash. Does the mark carry over?

- **Mark dies with hash.** Each minted definition has one mark, fixed at creation; an edit produces a new definition with a new mark. This blocks coincidental collapsing and nothing more.
- **Mark survives edits.** The substrate, or more accurately the editing layer, records "this definition descends from that one" by reusing the mark. The substrate then has two identity axes: content hash for what a definition is, mint mark for which thread of edits it belongs to. That axis is the natural home for editing history, update propagation, and the "rebind this name to the latest version of the thread" UX raised in `open-questions.md` §"Update strategies".

The second reading is more powerful and more dangerous. It defines a notion of "the same thing" distinct from hash equality, which the substrate has so far refused to do. The editing layer must reconcile the two: same mark with different hashes means versions of one thing, different marks mean two things. Different marks with the same hash cannot arise, since the mark is in the hash, though two authors minting independently could share an intent. This is entangled with the Grove direction in `07-hazel-substrate.md` lines 96–120, which names UID-based identity at the edit layer. Minted identity at the definition layer is the committed-layer counterpart of Grove's edit-layer UIDs; working out the relationship is deferred.

A synthesis from the update-strategies thread (`04-naming-layer.md` §"Threads under exploration" > "Update strategies") leans toward marks-survive but reframes the mint as the user's signal that a term has identity worth preserving across edits: intentional, not automatically accumulated lineage. The substrate exposes the grouping (which definitions share mint `m`) and the editor uses or ignores it. Synonyms-under-rebind, update cascades by mint thread, and term lineage across history then become substrate primitives instead of editor reconstructions. Open implementation questions: which gesture preserves the mint (rebinding an existing name, an explicit "edit of X" gesture, editing inside a patch anchored to existing bindings), and how an ingest with no prior binding distinguishes a fresh thing from an edit. This is the current direction of thinking, not a decision.

**Finding from p12 (2026-06-04).** Abstract types (`12-type-abstraction.md`) show that the mint plays two separable roles, and only one is the marks-survive question:

- **Distinctness** keeps coincidentally-equal definitions apart. It wants a fresh mark per authoring and is satisfied by dies-with-hash.
- **Lineage thread** ("the same conceptual thing, evolved") is the marks-survive reading, and is what update propagation, history and cross-version grouping depend on.

Abstract-type soundness does not depend on the mark. A representation change moves the witness, the witness is in the hash, so the new abstract type is distinct whatever its mark, and old and new values cannot mix (p12 asserts this: a value of the old representation is rejected by an op of the new one). Within one checkout, mark survival adds only lineage recognition, which the namespace binding already carries (`Counter.t → #T`, then `→ #T3`). It becomes load-bearing only across checkouts or branches, where the name is no longer an authoritative link, i.e. in the Grove/collaboration phase. p12 therefore takes dies-with-hash as the default and reserves marks-survive as the opt-in lineage axis for the collaboration prototype. This scopes the update-strategies synthesis without retracting it: marks-survive serves cascades and cross-checkout identity, not the soundness or distinctness of abstract types.

## Relation to existing concepts

- **Newtypes** (`03-content-addressing.md:118`). The "Hashing types as well as terms" thread names newtypes as distinct identity for an isomorphic carrier, which minted identity provides at the substrate level. Newtypes do it inside the type language with a constructor; minting does it for any definition without one. Whether one subsumes the other is open.
- **Procedure identity** (`02-definitions-and-derived-data.md:60`; `03-content-addressing.md:57–67`). Primitives and translators already have opaque identity baked into the hash: `int:add:v1` is a manually managed tag in the canonical AST at every call site. A mint mark plays that role for a user definition. The mechanism is the same; the population is broader.
- **Grove / collaborative editing** (`07-hazel-substrate.md:98–122`). Edit-layer UIDs are a roadmap item for collaborative editing; minted identity is their committed-layer counterpart. Related, with timelines kept separate.
- **Perkeep permanodes**, the closest existing implementation (found 2026-08-03). A permanode is a signed random value whose schema field is literally `"random"`, documented as "Any random string, to force the digest of this node to be unique". It anchors a mutable thing in an immutable content-addressed store, with mutation carried by separate signed `claim` blobs replayed in order. It differs from this design in two ways. It holds no content, only identity, with content in the claims that point at it; this is the "separate constructor" option in the *Where the mint mark lives* sub-question below, shipped and load-bearing. And it is signed, so mint-as-keypair is someone's default rather than a deferred hardening step. Cited in `../related-work/01-content-addressing.md` §"Minting".
- **Issued identifiers, and the word "mint."** Persistent-identifier systems use *mint* literally: ARKs are minted by a Name Assigning Authority, DOIs via DataCite and Crossref. Di Cosmo, Gruenpeter & Zacchiroli call an identifier built from the object itself **intrinsic**; an issued identifier is asserted by an authority and checkable only by asking it. A mint mark is an issued identifier embedded in an intrinsic one: the one part of the hash that cannot be recomputed from the definition, only read back. This is sharper than "an opt-in axis of identity orthogonal to content addressing", and it explains the cost noted in `../related-work/01-content-addressing.md` §"Minting": a minted definition cannot share derived aspects with its structural twin, so it is type-checked and evaluated twice. *Intrinsic* is their term; *extrinsic* is not, as their paired distinction is DIO vs. IDO (details in `../related-work/01-content-addressing.md`).
- **Encoding derivation in the label is a known open issue.** Di Cosmo et al.: "An object may be used to create a new object that is a modification of the first one, and one may want to keep track of the fact that the second one is derived from the first one. Some systems of identifiers allow to encode this versioning information *in the object label*." They flag it without solving it. A mint does this, so the coincidental-convergence hazard sits on a recognized gap in identifier design.
- **Unison's `unique`, and per-sort opt-in.** Unison mints types but not terms; plain `type` is minted and `structural type` is the marked opt-out. That is neither granularity in the *Per-definition vs. per-language opt-in* thread above but a third, **per-sort**, which is also where this project converged: p16/p17 mint only labels, p12 mints `Opaque` types, terms stay structural. Source-verified 2026-08-07, replacing an earlier inference from rendered output: a declaration with no modifier calls `resolveUniqueTypeGuid` (`parser-typechecker/src/Unison/Syntax/DeclParser.hs:230`), and the GUID enters the hash as `[Tag 1, Text guid]` (`unison-hashing-v2/src/Unison/Hashing/V2/DataDeclaration.hs:130-132`). The pretty-printer drops the keyword (`DeclPrinter.hs:304`), which is what the earlier inference saw. Details in `../systems/unison/04-caching-update-merge.md` §"Minting".
- **What minting costs to maintain, from a decade of Unison.** A mint must survive re-saving its definition, or an edit silently creates a new type and breaks every dependent. Unison has no explicit *edit-of-X* gesture, so it reconstructs intent at save time by looking the type's name up in the namespace and reusing the GUID it finds; where the name is conflicted, "an arbitrary one is chosen" (`parser-typechecker/src/Unison/Codebase/UniqueTypeGuidLookup.hs:19-20`). Merge needs more: a `namespace_unique_type_guid` side table (`codebase2/codebase-sqlite/sql/016-add-merge-branch-tables.sql`) and a `makeUniqueTypeGuids` pass, so two branches do not mint independently for one name. This is evidence for the explicit-gesture design in *Marks across content edits* below, since the gesture removes the guesswork Unison pays for with a rename hazard and an arbitrary tiebreak. It is also evidence that the design is incomplete, since the case that forced Unison's side table is merge, which p11's threads do not address. Unresolved; see `../systems/unison/07-arbor-mapping.md` §"Mint".
- **Naming layer** (`04-naming-layer.md`). A mint mark is not a name. Names are human strings bound to hashes; marks are opaque, internal and inside the canonical AST.

## Non-goals (current phase)

- **No editing-history machinery.** Marks-survive is the natural place for history, but edit-history storage, query and UI are out of scope.
- **No update-propagation mechanism.** No-silent-breakage still holds, and minting introduces no automatic caller migration. `open-questions.md` §"Update strategies" tracks propagation.
- **No migration policy.** Adding minted identity later need not be transparent to existing prototype state; per `00-overview.md` §"Strategic posture", bootstrap accepts full state rebuilds.
- **No prototype prescription.** The doc left open which prototype should exercise minting first, suggesting one where two domain types share a carrier and the structural collapse is visibly wrong in the UI.

## Open sub-questions

Tracked in `open-questions.md` under "Minted identity."

- **Per-definition vs. per-language opt-in.** See above.
- **Marks across content edits.** Whether marks survive content edits. See above.
- **How marks are minted.** Random UID, monotonic counter, derived from creation context, or something else. The choice affects reproducibility (does a fresh ingest of the same source produce the same mark?) and what "fresh mint" means under re-import or migration. The nonce framing reduces it to counter (needs a coordination point) vs. random (does not). Content-derived marks are ruled out, since they would collapse exactly where minting exists to prevent collapse.
- **Where the mint mark lives in `Definition.t`.** A field on every `Definition.t` with a "no mark" sentinel for structural definitions, a separate constructor (`Definition.t = Structural | Minted(mark, …)`), or an aspect with a hash-stable contract. Each is workable, with different consequences for canonicalization code and for how aspects interact with mintedness.
- **Interface implications.** Whether and how to show minted vs. structural in the UI (a namespace-tree badge, a colour in the detail pane, nothing). Bound up with the editing gesture in the per-definition reading.

## Connections

- `03-content-addressing.md` §"Hash input" — per-language canonicalization is the existing dial; minting is another axis on it.
- `03-content-addressing.md` §"Hashing types as well as terms" — newtypes, the closest existing concept.
- `02-definitions-and-derived-data.md` §"Procedure identity" — the same maneuver for primitives.
- `04-naming-layer.md` — names stay separate from identity; minting strengthens the separation.
- `07-hazel-substrate.md` §Grove — committed-layer counterpart to Grove's edit-layer UIDs.
- `open-questions.md` §"Update strategies" — where "follow the latest version of this mint thread" would be called.
- `11-label-sort.md` — labels are the first sort minted by default, a partial answer by example to the opt-in granularity thread and an instance of the per-sort option.
- `../related-work/01-content-addressing.md` §"Minting — identity that is not a function of content" — prior art: Perkeep permanodes, Unison's `unique`, ARK/DOI minting and the intrinsic/extrinsic framing, the nonce-and-commitment construction, Nix input-addressing as the cost argument, and the negative finding that IPFS and IPLD have no first-class equivalent.
- `../systems/unison/04-caching-update-merge.md` §"Minting" — Unison's `unique` read in source: where the GUID comes from, how it enters the hash, and what keeping it stable costs under edit and merge.
