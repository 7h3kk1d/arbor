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

### The mark is a nonce

The precise name for what the mint mark is: a **nonce** — a number used once, mixed into the hash preimage to force distinct digests. Saying it that way imports a well-understood construction (the salt in a salted hash commitment) and settles three things that otherwise look like open design space.

- **Binding comes free; minting cannot weaken the store.** A commitment scheme's *binding* property is exactly what the content hash already gives us, and adding a nonce to the preimage does not touch it. So minting changes only *which definitions fall into the same equivalence class*. It cannot affect no-dangle, immutability, or the primacy of `Hash → definition` — those follow from binding, not from the equivalence.
- **We want distinctness, not hiding — so the mark can be public.** Cryptography adds a nonce for the opposite reason: to stop an observer recovering a low-entropy value, which forces the salt to be secret and high-entropy. We add it only so coincidentally-equal definitions do not collapse, and that needs nothing but uniqueness. The mark is therefore safe to expose — renderable in a UI, loggable, comparable. This is also why the **mint-as-keypair** escalation in `open-questions.md` §"Type abstraction" is a real change of kind rather than a refinement: it introduces a *secrecy* requirement that distinctness alone never had.
- **"Once" is the entire requirement on the generator.** That resolves the shape of the *How marks are minted* sub-question below. The two standard answers are a counter (certainly unique, but needs coordinated state) or randomness (unique with overwhelming probability, needs no coordination). In a distributed or multi-agent setting the second is the conventional pick for exactly the reason we care about — no coordination point — which also makes marks non-reproducible under re-ingest by construction rather than by accident.

### The coincidental-convergence hazard

The motivating example in §"Purpose" is Meters-vs-Feet — two concepts that are coincidentally identical *at authoring*. There is a second, sharper case that only appears once definitions are edited over time, and it is the one that most clearly requires the mark to be in the hash.

Two distinct concepts `A` and `B` are each edited, and both happen to arrive at structurally identical content `C`. Under content addressing alone the two histories **collapse at C** — there is one `C`, and which of `A` or `B` it descends from is not recoverable. Edit once more and `C ⇒ A'`, `C ⇒ B'` cannot be attributed: two independent threads have been welded together by an accident of structure, and the accident is *silent*. A fresh mark per authoring prevents the weld, because `C` authored from `A` and `C` authored from `B` are simply different definitions.

**What this case does and does not argue for.** It is satisfied by the *dies-with-hash* reading: freshness alone breaks the collapse, and the thread `A → C → A'` is then carried by the namespace binding or an edit log, not by the mark itself. So it is evidence *for* the p12 position below rather than for marks-survive — marks-survive would only add the convenience of grouping by mark without consulting the log. What the case does establish is that **the mark must be inside the hash rather than beside it**, since the whole hazard is that two definitions would otherwise be one Store entry.

Worth knowing that this hazard has a cheaper answer for the lineage half specifically, which we are not taking: Git preserves exactly this distinction with **no nonce at all**, because a commit's hash includes its parents — `A ⇒ C` and `B ⇒ C` yield two distinct commits sharing one deduplicated tree. Under minting, `C_a` and `C_b` are two definitions and so get type-checked and evaluated twice; under a predecessor-carrying version node they would share one set of aspects. That cost is real and is the reason mint-everything is not free. It does not rescue the *distinctness* role, however: a parent pointer lives in a separate layer, so `Ref(hash-of-C)` cannot tell which concept it denotes, whereas a mark in the hash makes every reference carry the distinction. Prior art and the fuller comparison — the ABA problem, surrogate-vs-natural keys and Type 2 slowly-changing dimensions, why CRDTs cannot content-address their elements — are in `../related-work/01-content-addressing.md` §"Coincidental convergence".

The framing has a limit worth stating: a nonce buys *distinctness* and nothing else. It says two definitions are not the same; it does not say two definitions are versions of one thing. That is why the marks-survive thread below is a genuinely separate question and not a detail of how the mark is generated — reusing a nonce is precisely the thing the construction forbids, so "marks survive edits" is a deliberate abuse of the mechanism, and should be understood as such.

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

A working synthesis from the update-strategies thread (`04-naming-layer.md` §"Threads under exploration" > "Update strategies") leans toward the marks-survive reading, but reframes what the mint *is*: the user's signal that this term has identity worth preserving across edits — intentional, not automatic accumulated lineage. The substrate exposes the grouping (which definitions share mint `m`); the editor uses or ignores it as appropriate to the situation. Under that reading, synonyms-under-rebind, update-cascade-by-mint-thread, and term-lineage-across-history all become substrate primitives rather than editor reconstructions. Several implementation questions remain open — most importantly, what gesture preserves the mint (rebinding an existing name? an explicit "edit of X" gesture? editing inside a patch anchored to existing bindings?), and how a new ingest with no prior binding distinguishes "fresh thing" from "edit of an existing thing." The reading is a leaning recorded as the substrate's current direction of thinking, not a decision; no prototype has exercised it.

**Finding from p12 (2026-06-04).** Exercising abstract types (`12-type-abstraction.md`) sharpens that the mint plays *two separable roles*, and only one of them is the marks-survive question:

- **Distinctness** — keeping two coincidentally-equal definitions apart — wants a *fresh* mark per authoring and is satisfied by the simple "dies with hash" reading.
- **Lineage thread** — "this is the same conceptual thing, evolved" — is the marks-survive reading, and it is what update propagation / history / cross-version grouping live on.

The key observation is that abstract-type **soundness does not depend on the mark at all**. A representation change moves the witness, and the witness is in the hash, so the new abstract type is a distinct type *regardless of its mark* — old and new values cannot mix whether the mint is carried or fresh (p12 asserts this directly: a value of the old representation is rejected by an op of the new one). So nothing in the type system needs marks to survive. Within a single checkout, the only thing mark-survival adds is lineage *recognition*, and that is already carried by the namespace binding (`Counter.t → #T` then `→ #T3`). Mark-survival becomes load-bearing only **across checkouts/branches**, where the name is no longer an authoritative link — i.e., the Grove/collaboration phase. p12 therefore takes **dies-with-hash as the default**, with marks-survive reserved as the opt-in lineage axis for the collaboration prototype. This does not retract the update-strategies synthesis above — it scopes it: marks-survive earns its keep for cascades and cross-checkout identity, not for the soundness or distinctness of abstract types.

## Relation to existing concepts

- **Newtypes** (`03-content-addressing.md:94–95`). The "Hashing types as well as terms" thread already names newtypes as a related concept: a newtype is "distinct identity for an isomorphic carrier," which is exactly what minted identity provides at the substrate level. Newtypes solve it inside the type-language with a constructor; minted identity generalizes the maneuver to any definition without requiring a constructor in any specific language. We leave open whether one subsumes the other or whether they coexist.
- **Procedure identity** (`02-definitions-and-derived-data.md:67`; `03-content-addressing.md:38–50`). Procedure tags already give primitives and translators opaque-identity-baked-into-hash: `int:add:v1` is identified by a manually-managed tag, and the tag is part of the canonical AST at every call site. Minted identity extends the same maneuver to user-created definitions — the mint mark plays, for a user definition, the role the manually-versioned tag plays for a primitive. The mechanism is not new; the population is broader.
- **Grove / collaborative editing** (`07-hazel-substrate.md:96–120`). UIDs at the edit layer are already on the roadmap as a future direction for collaborative editing. Minted identity at the definition layer is the committed-layer counterpart. Cited as related; timelines deliberately not entangled.
- **Perkeep permanodes** — the closest existing implementation, found 2026-08-03. A permanode is a cryptographically signed random value whose schema field is literally `"random"`, documented as "Any random string, to force the digest of this node to be unique"; it anchors a *mutable* thing inside an immutable content-addressed store, with mutation carried by separate signed `claim` blobs replayed in order. Two structural differences from the design above are worth weighing. A permanode holds **no content at all** — it is a pure identity token, with content living in the claims that point at it — which is the "separate constructor" option in the *Where the mint mark lives* sub-question below, shipped and load-bearing. And it is **signed**, i.e. the mint-as-keypair escalation is already someone's default rather than a deferred hardening step. Cited in `../related-work/01-content-addressing.md` §"Minting".
- **Issued identifiers, and the word "mint."** The persistent-identifier world uses *mint* as its literal verb — ARKs are minted by a Name Assigning Authority, DOIs via DataCite and Crossref. Di Cosmo, Gruenpeter & Zacchiroli call an identifier built *from the object itself* **intrinsic**; an issued identifier is the complement, asserted by an authority and checkable only by asking it. A mint mark is then best characterized as *an issued identifier smuggled inside an intrinsic one*: the one part of the hash that cannot be recomputed from the definition, only read back out. That is a sharper statement of what minting does than "an opt-in axis of identity orthogonal to content addressing," and it explains the cost — see the early-cutoff note in `../related-work/01-content-addressing.md` §"Minting", where a minted definition cannot share derived aspects with its structural twin and so gets type-checked and evaluated twice. (*Intrinsic* is their term; *extrinsic* is not — their paired distinction is DIO vs. IDO. Details in `../related-work/01-content-addressing.md`.)
- **Encoding derivation in the label is a recognized open issue, not an arbor peculiarity.** Di Cosmo et al. raise it directly: "An object may be used to create a new object that is a modification of the first one, and one may want to keep track of the fact that the second one is derived from the first one. Some systems of identifiers allow to encode this versioning information *in the object label*." They flag it and do not solve it. That is exactly what a mint does, and it means the coincidental-convergence hazard above sits on a known gap in identifier design.
- **Unison's `unique`, and a third answer to the opt-in granularity question.** Unison mints **types but not terms**, and (on the evidence of its own pretty-printer output, though the docs never say so in words) plain `type` is the minted case with `structural type` as the marked opt-out. That is neither of the two granularities in the *Per-definition vs. per-language opt-in* thread below — it is **per-sort**, which is also where this project already converged: p16/p17 mint only labels, p12 mints `Opaque` types, terms stay structural. Worth adding to that thread as a live option rather than leaving it implicit in the prototypes.
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
- **How marks are minted.** Random UID, monotonic counter, content-derived from creation context, something else? The choice has consequences for reproducibility (a fresh ingest of the same source produces the same mark? different marks?) and for what "fresh mint" means under re-import or migration. Narrowed by the nonce framing above: *uniqueness is the only requirement*, so this reduces to counter (needs a coordination point) vs. random (does not), and content-derived is ruled out — a mark derived from content would collapse exactly where minting exists to prevent collapse.
- **Where the mint mark lives in `Definition.t`.** A first-class field on every `Definition.t` (with structural definitions carrying a sentinel "no mark" value), a separate constructor (`Definition.t = Structural | Minted(mark, …)`), or an aspect with a hash-stable contract? Each is workable; the choice has implications for canonicalization code and for how aspects interact with mintedness.
- **Interface implications.** How (or whether) to surface minted-vs-structural at the UI layer. A badge on the namespace tree, a different color in the detail pane, nothing at all? Bound up with the editing gesture in the per-definition reading.

## Connections

- `03-content-addressing.md` §"Hash input" — per-language canonicalization is the existing dial; minting sits naturally as another axis on it.
- `03-content-addressing.md` §"Hashing types as well as terms" — newtypes are the closest existing concept and would compose with minted identity once types are first-class.
- `02-definitions-and-derived-data.md` §"Procedure identity" — minted identity is the same maneuver generalized to user definitions.
- `04-naming-layer.md` — names remain separate from identity; minting strengthens that separation rather than weakening it.
- `07-hazel-substrate.md` §Grove — committed-layer counterpart to Grove's edit-layer UIDs.
- `open-questions.md` §"Update strategies" — natural callsite for "follow latest version of this mint thread" once minting is concrete.
- `11-label-sort.md` — labels are the first concrete sort minted by default; partial answer-by-example to the per-definition vs. per-language opt-in thread above, and in hindsight an instance of the per-sort option Unison also takes.
- `../related-work/01-content-addressing.md` §"Minting — identity that is not a function of content" — the prior art: Perkeep permanodes, Unison's `unique`, ARK/DOI minting and the intrinsic/extrinsic framing, the nonce-and-commitment construction, Nix input-addressing as the cost argument, and the negative finding that IPFS and IPLD have no first-class equivalent.
