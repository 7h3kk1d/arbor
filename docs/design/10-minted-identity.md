# Minted Identity

**Status:** Draft. Exploration — no prototype has exercised this yet.

## Purpose

The substrate as designed identifies every definition by its content hash alone. α-equivalence canonicalization (`03-content-addressing.md` §"Hash input") is a deliberate amplifier of this: two definitions that differ only in bound-variable names collapse to the same hash, the same Store entry, and the same aspect rows. Structural sharing is the point.

There are cases where that collapse is wrong. Two definitions can be *coincidentally* structurally identical yet mean different things to their authors. Unison's archetypal example is `unique type Meters` vs. `unique type Feet`, both `Int`-shaped: the carrier is the same, the intent is not. The same pattern shows up for terms (two zero-argument constants both equal to `0`; two empty record values), for tags and enum cases (two singleton constructors with no payload), and — looking further out — for any situation where the substrate's identity needs to survive content edits so editing history and update-propagation can be attached to it.

This doc opens the thread on *minted identity*: an opt-in axis of identity, orthogonal to and composable with content addressing, baked into the canonical form so that minted definitions are not deduplicated against coincidentally-equal structural neighbors.

## The core idea

A created definition may be *minted*. At creation time the substrate stamps it with a *mint mark*: an opaque, non-stringly-typed value drawn fresh from a generator the user does not control. The mark participates in the canonical AST fed to the hash function. Two definitions identical in every other respect but differing in mint mark therefore receive *different* hashes — they will not deduplicate.

Definitions that are not minted remain *structural*, exactly as the substrate behaves today. Minted-ness is an opt-in property of a definition, not a global mode.

Important properties to preserve:

- **The mark is opaque.** Not a string, not a name, not anything the user types. The mint generator is the only source. The human-rendered name lives independently in the naming layer, exactly as for structural definitions — minted identity does not reintroduce the name-identity coupling that `04-naming-layer.md` removed.
- **The hash remains primary.** Adding a mint mark does not give us a second lookup key; it just changes which definitions collapse and which don't. `Hash → definition` is still globally unique, and every cross-definition reference inside a stored program is still a hash reference (`03-content-addressing.md` §"References in stored programs").
- **Uses look the same as structural uses.** A reference to a minted definition is `Ref(hash)` just like any other. The mark is part of the *referenced* definition's content, not part of the call site.

## Where the mint mark enters

Following the surface→internal→node discipline used elsewhere:

- **Surface.** A creation gesture in the editing layer (a language-specific keyword analogous to Unison's `unique`, or a substrate-level flag at ingest, or a per-language always-mint declaration — see threads below) signals "this definition is minted."
- **Internal AST.** The definition carries a `Mint(mark)` field (or constructor) alongside its body. Structural definitions carry no such field.
- **Node encoding.** The canonical byte sequence prepends or includes the mint mark for minted definitions; structural definitions encode unchanged. Two structural definitions hash as they always did. A minted definition hashes differently from its structural twin, and from every other minted definition with a different mark.
- **Re-ingestion.** Round-tripping a minted definition through serialization preserves its mark — the mark is content, not metadata.

The mint mark is *not* an aspect. Aspects are associated data outside the hash; the mint mark is inside the hash by construction. (Whether the *mapping* from mint mark to some richer record — author, creation time, history — lives in the aspect store is a separate question, and one the substrate would handle the usual way.)

## Threads under exploration

### Per-definition vs. per-language opt-in

There are two natural granularities and we have not picked one:

- **Per-definition** (Unison-style). Each definition flags itself: `unique X = …` mints, plain `X = …` does not. Granularity is fine, the user is in control, but every language that supports minting must surface the gesture.
- **Per-language**. A language declares in its canonicalizer that all of its definitions are minted (or none are). This parallels the existing per-language canonicalization choice in `03-content-addressing.md` §"Hash input" — α-equivalence collapsing is already a per-language decision, and minting could ride the same dial. Simpler; less granular; forces every author in language L into the same regime.

Both readings could coexist: per-language sets a default, per-definition overrides. Left open. The choice probably depends on whether minting ends up being a language-design feature (Meters-vs-Feet) or a substrate-infrastructure feature (every definition gets a stable identity-thread for history). The two readings push opposite directions.

### Marks that survive content edits

The harder thread. Suppose a user edits a minted definition. By construction the edit produces a new hash. Does the *mark* carry over into the new hash?

- **Mark dies with hash.** Each minted definition has exactly one mark, fixed at creation; an edit produces a new minted definition with a new mark. This is the simple reading. It blocks coincidental collapsing and nothing more.
- **Mark survives edits.** The substrate (or, more honestly, the editing layer above it) tracks "this new definition is the descendant of that old one" by reusing the mint mark. Now the substrate has a second identity axis — content hash for "what it is" and mint mark for "which thread of edits it belongs to" — and that axis is the obvious home for editing history, update propagation, and the kind of "rebind name to follow latest version of this thread" UX that `open-questions.md` §"Update strategies" raises.

The second reading is more powerful and more dangerous. It introduces a notion of "the same thing" distinct from hash equality, which the substrate has so far refused to define. The editing layer must reconcile the two notions: two definitions with the same mark but different hashes are versions of one thing; two with different marks but the same hash (impossible in the strict reading — the mark is in the hash — so this case can't arise structurally, but the *intent* of two authors minting independently could coincide) are two things. This is entangled with the Grove direction in `07-hazel-substrate.md` lines 96–120, which already names UID-based identity at the edit layer as a future direction. Minted identity at the definition layer is the committed-layer counterpart of Grove's edit-layer UIDs; the relationship is worth working out, but not now.

## Relation to existing concepts

- **Newtypes** (`03-content-addressing.md:94–95`). The "Hashing types as well as terms" thread already names newtypes as a related concept: a newtype is "distinct identity for an isomorphic carrier," which is exactly what minted identity provides at the substrate level. Newtypes solve it inside the type-language with a constructor; minted identity generalizes the maneuver to any definition without requiring a constructor in any specific language. We leave open whether one subsumes the other or whether they coexist.
- **Procedure identity** (`02-definitions-and-derived-data.md:67`; `03-content-addressing.md:38–50`). Procedure tags already give primitives and translators opaque-identity-baked-into-hash: `int:add:v1` is identified by a manually-managed tag, and the tag is part of the canonical AST at every call site. Minted identity extends the same maneuver to user-created definitions — the mint mark plays, for a user definition, the role the manually-versioned tag plays for a primitive. The mechanism is not new; the population is broader.
- **Grove / collaborative editing** (`07-hazel-substrate.md:96–120`). UIDs at the edit layer are already on the roadmap as a future direction for collaborative editing. Minted identity at the definition layer is the committed-layer counterpart. Cited as related; timelines deliberately not entangled.
- **Naming layer** (`04-naming-layer.md`). The mint mark is *not* a name. Names continue to be human strings bound to hashes; mint marks are opaque, internal, and live inside the canonical AST. Minted identity therefore does not reintroduce the name-identity coupling that the naming layer carefully separates.

## Non-goals (current phase)

- **No editing history machinery.** The "marks survive edits" reading is the natural place for editing history, but actually implementing edit-history storage, query, and UI is not in scope here.
- **No update-propagation mechanism.** Related: the substrate's no-silent-breakage property continues to hold. Minting does not introduce automatic migration of callers. `open-questions.md` §"Update strategies" tracks the propagation question separately.
- **No migration policy.** No commitment that adding minted identity later will be transparent to existing prototype state. Per `00-overview.md` §"Strategic posture," we accept full state rebuilds during bootstrap.
- **No prototype prescription.** Which prototype, if any, should exercise minting first is left open. Plausible candidate: a future prototype where two distinct domain types share a carrier and the structural collapse becomes visibly wrong in the UI.

## Open sub-questions

Tracked in `open-questions.md` under "Minted identity."

- **Per-definition vs. per-language opt-in.** Sharpened above.
- **Marks across content edits.** Whether mint marks survive content edits of the same definition. Sharpened above.
- **How marks are minted.** Random UID, monotonic counter, content-derived from creation context, something else? The choice has consequences for reproducibility (a fresh ingest of the same source produces the same mark? different marks?) and for what "fresh mint" means under re-import or migration.
- **Where the mint mark lives in `Definition.t`.** A first-class field on every `Definition.t` (with structural definitions carrying a sentinel "no mark" value), a separate constructor (`Definition.t = Structural | Minted(mark, …)`), or an aspect with a hash-stable contract? Each is workable; the choice has implications for canonicalization code and for how aspects interact with mintedness.
- **Interface implications.** How (or whether) to surface minted-vs-structural at the UI layer. A badge on the namespace tree, a different color in the detail pane, nothing at all? Bound up with the editing gesture in the per-definition reading.

## Connections

- `03-content-addressing.md` §"Hash input" — per-language canonicalization is the existing dial; minting sits naturally as another axis on it.
- `03-content-addressing.md` §"Hashing types as well as terms" — newtypes are the closest existing concept and would compose with minted identity once types are first-class.
- `02-definitions-and-derived-data.md` §"Procedure identity" — minted identity is the same maneuver generalized to user definitions.
- `04-naming-layer.md` — names remain separate from identity; minting strengthens that separation rather than weakening it.
- `07-hazel-substrate.md` §Grove — committed-layer counterpart to Grove's edit-layer UIDs.
- `open-questions.md` §"Update strategies" — natural callsite for "follow latest version of this mint thread" once minting is concrete.
- `11-label-sort.md` — labels are the first concrete sort minted by default; partial answer-by-example to the per-definition vs. per-language opt-in thread above.
