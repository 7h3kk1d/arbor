# Type Abstraction

**Status:** Draft. Exploration — no prototype has exercised modules, signatures, or existentials yet. The substrate direction below is a set of *leanings* worked out in design discussion, not commitments; the natural prototype trigger is the TAPL-Ch.-24 (existentials) / Ch.-29 (F-omega) region, well past the current frontier.

## Purpose

The substrate needs *type abstraction*: the ability to write logic generically over types whose concrete representation is hidden. Two motivating populations:

- **Generic algorithms over an interface.** An algorithm over stacks should not care whether the stack is an array, a linked list, or something else — only that it provides `push`/`pop`/`empty` of the right types. This is the *functor* shape: code parameterized over an implementation, with the carrier type abstract *inside the parameterized body*, the caller choosing the implementation.
- **Algebraic structures.** A monoid is a carrier `A` with `id : A` and `op : A → A → A`. Code over "any monoid" must obtain the operations — including `id`, a *value* of the abstract carrier — **without** a pre-existing value of `A`. This is the *existential package* shape: `∃A. { id : A, op : A → A → A }`; unpacking the package hands you `id` directly.

These are two quantifiers (`∀` for the functor, `∃` for the package) over the **same underlying interface shape**, and that shape is the bridge to the rest of arbor: a signature is an existential (or universal) binder wrapped around a `11-label-sort.md` **label-record**.

### Why content-addressing alone is not enough

A hash is already an opaque handle, so it is tempting to think content-addressing *is* abstraction. It is not, for two reasons that recur throughout this doc:

1. **Generic code cannot name the concrete operation by hash.** An algorithm written against "any stack" must reference `push` *before any particular stack exists*. It cannot cite a concrete `push`'s hash, because that hash varies per implementation. It references the **label** `push` (a label hash, per `11`), and the implementation supplies the binding at use. Content addressing identifies *the* `push`; abstraction needs *a* `push`-shaped slot.
2. **Hiding a representation is a typing judgment, not a storage fact.** That a value's representation is `Int` is visible in its type unless the type system actively refuses to expose it. Sealing is what performs that refusal, and it lives in the type system (below).

## The two quantifiers, concretely

The same label-record signature carries both readings:

```
StackOps R = { empty : R, push : Int → R → R, pop : R → Option (Int × R) }
```

- **Functor (`∀`, genericity over implementations).** A stack algorithm is a functor `∀R. StackOps R → Client`. The implementation is supplied (now or by a caller); `R` is abstract in the body. This is the primary genericity story.
- **Existential package (`∃`, hiding a representation).** A first-class stack value has type `∃R. StackOps R`. A function receiving one `unpack`s it and cannot observe `R`. A monoid `∃A. { id : A, op : A → A → A }` is the same shape; unpacking yields `id` without needing a value of `A`.

The label-record is shared; only the outer binder differs. The doc's spine is: **signature = binder + label-record.**

## Abstract-type identity

The central design question is what an abstract type's identity *is*, given content addressing. The resolved leaning:

> **An abstract type is a content-addressed `Type` definition whose content is `opaque_tag ++ mint_mark ++ witness_ref`.**

Three ingredients, each load-bearing:

- **Witness in the hash — for soundness.** If the representation (witness) changes, the abstract type's identity *must* change, or values built against the old and new representations can be mixed. Concretely: seal a stack over a linked list (`push_v1 : Int → t → t`), then edit the representation to an array (`push_v2 : Int → t → t`); if `t`'s identity were stable across the edit, `push_v2 3 empty_v1` would typecheck and apply an array operation to a list value. So the witness is *inside* `t`'s hash. This satisfies the purity requirement — same content hash ⇒ same meaning; abstract-type identity is a content hash, not a side channel.
- **Mint mark — for distinctness.** Without it, two independently-authored abstractions whose witnesses coincide would collapse. `Counter` over `Int` and `Celsius` over `Int` would both encode as `hash(opaque_tag ++ Int)` and the type system would treat a temperature and a counter as the same type. The mint mark (per `10-minted-identity.md`) breaks the tie. Every authoring gesture draws a fresh mark, so independently-authored abstractions **never** share a mint; the same mark persists only down one definition's edit lineage. The mint mark is *necessary* precisely in the one case content alone cannot distinguish — coincident witnesses — and is automatic everywhere else.
- **Opaque tag — for hiding.** The external typechecker refuses to unfold `t` to its witness; only the *defining module's own* checking unfolds it (to verify the operations). The opaque tag is the marker that says "do not expose the witness here."

This makes type abstraction a third concrete population minted by definition (after `11`'s labels), and partly answers `10`'s per-definition-vs-per-language opt-in thread by example: abstract-type-bearing modules mint on authoring.

### Generative vs. applicative, mapped onto authoring vs. computation

The classic ML axis (SML generative sealing vs. OCaml applicative functors) maps cleanly onto arbor's authoring/computation distinction:

- **Structure sealing is generative-by-authoring.** Authoring a module with an abstract type is an authoring gesture and draws a fresh mint. Two independently-authored modules with identical implementations are distinct because each minted separately. Identity collapse happens only when it is literally the same definition (or an edit-descendant carrying the mint forward).
- **Functor application is applicative by default.** Applying a functor is a *computation*, not an authoring gesture; no mint is drawn. The result is content-addressed by `(functor_hash, argument_hash)`, so `F(X)` always yields the same module hash and `F(X).t = F(X).t` for free. Generativity-on-application would require the functor to mint on each call, which fights the substrate's pure-computation model. Authoring a *fresh* `F(X)` in the editor is still an authoring gesture and mints; and an explicit unit parameter (`F() :> SIG`, OCaml-style) is available to force generative application on demand.

## Sealing and translucency

Sealing lives in the type system. Following the surface→internal→node discipline:

- **Signatures are content-addressed `Type` definitions.** A signature is a `11`-style label-record of *components*. A **type-component** declares a label at kind `Type`; it may be **opaque** (`type t`) or **transparent / manifest** (`type t = Int`) — the translucent-sum design of Harper–Lillibridge and Leroy. A **value-component** declares a label with a type that may reference the type-component labels by hash. A signature is witness-free: it is the interface, not an implementation. Its hash contains label hashes and type structure, no names (per `04`).
- **A structure is the implementation** — a label-record binding the same labels to concrete types (`t = Int`) and terms. Transparent: the witness is visible.
- **Ascription `M :> S` is the sealing operation.** It checks `M` against `S`, mints a fresh abstract-type definition (`opaque ++ mint ++ witness`) for each opaque type-component of `S`, and produces a sealed module whose exported operations are typed over the minted abstract types. Transparent components keep their equations (the equation `t = Int` is part of the signature's content; the type stays exposed). Translucency is therefore per-component: a signature can expose some types and hide others.
- **Node encoding.** Exported operations are *sealed wrappers* — distinct `Seal{ ty, impl }` nodes, where `impl` is the raw implementation (a generic term typed over the witness, structurally shared like any other term) and `ty` is the external type (over the abstract-type hash). The wrapper's own `Type_of` is `ty`; the raw body's `Int → Int`-style type is nested, not exposed. A sealed export therefore has a different hash from its raw body, and from every other term that happens to share that body. The witness↔abstract-type correspondence is known only inside the module's own typecheck and is never exported.
- **Pretty-printing.** Abstract types and sealed operations reverse-look-up through the namespace and render by name (`Counter.t`, `Counter.incr`), falling back to short hashes when unbound — paralleling `11` and p9.

`public`/`private` (below) and information hiding are *entirely subsumed* by ascription: a private component is one the implementation defines but the ascribed signature omits.

### Opacity is a typing discipline, not secrecy

Content addressing makes everything in the store reachable: a client can follow an abstract type's `witness_ref` and learn the representation, and can read a sealed operation's raw body. Opacity does **not** depend on hiding any of those bytes. It is a property of the typing *rules*: the checker grants the equation `t ≡ witness` to **no one** who merely references the abstract-type hash. The witness is in the hash for *identity and soundness* (a different representation must yield a different hash); it is consulted for *unfolding* in exactly one place — inside the structure that creates the abstract type.

The lifecycle that makes this forgery-safe:

1. **Author a structure.** `struct { type t = W; … } :> SIG` is checked with `type t = W` *transparent and local* — `t` here is a local binder, not yet the eventual abstract-type hash. The operations type-check against the witness.
2. **Seal.** The seal mints a fresh abstract type `#A = opaque ++ mint ++ witness:W`, substitutes the local `t ↦ #A` in the operations' types, and emits the sealed-export nodes (types now over `#A`), the module record, and the standalone `#A`. These outputs are trusted because they were just checked against the transparent witness.
3. **Forever after, `#A` is opaque.** No definition that *references* `#A` may unfold it. Sealed exports carry their external types as established-once data; they are not re-derived against `#A`'s witness on use.

Two consequences pin the safety:

- **Sealed exports are outputs of a validated structure, not independently-ingestable nodes.** A forger who writes `Seal{ #A → #A, <anything> }` by hand cannot get it past ingest, because ingest does not unfold `#A` on their behalf — `<anything> : W → W` does not check against the opaque `#A → #A`.
- **Re-authoring the structure mints a *fresh* abstract type.** `struct { type t = W; … } :> SIG` authored again produces `#A' ≠ #A` (new authoring ⇒ new mint), so a forger gets their own abstraction, never values of the original `#A`. The only route to an `#A` value is the original module's own sealed operations.

This is ML generativity recast in content-addressed terms, and it is why witness-in-hash (soundness) and opacity (safety) never collide: they are exercised at different moments, by different parties — the witness is unfolded once, locally, at sealing; opacity is the standing rule everywhere else.

## Unbundled abstract types and the opacity trilemma

*Design exploration, 2026-06-04. This section isolates abstract types from the module/functor machinery above — one abstract type and its operations, no functor, no first-class module value. Functors and first-class modules stay out of scope here; the dependency-model and subtyping sections below cover those.*

Considered in isolation, the `Module{}` record is unnecessary. The abstraction is carried entirely by three independent pieces, with naming done separately:

- **One `Type` node** `opaque(mint, witness)` — the mint for distinctness, the witness in the hash for soundness, exactly as above.
- **Independent per-operation sealed-wrapper nodes** `open #A in E : T` — each its own definition, where `E` is the raw implementation (typed over the witness) and `T` is the external type (over `#A`). `open` is the local, controlled unfold: inside it the checker treats `#A ≡ witness`, and the ascription `: T` re-seals at the boundary. This *is* the `Seal{ ty, impl }` node of the worked example, with the unfold made explicit rather than implied by a constructor.
- **A separate name → hash table** with no bearing on type identity.

| hash | sort | content | external `Type_of` |
|------|------|---------|--------------------|
| `#t` | Type | `opaque(mint:m1, witness:#Int)` | (is a type; displays `IntCounter.t`) |
| `#raw` | Term | `\. $0 + 1` | `Int → Int` (raw body, freely shared) |
| `#empty` | Term | `open #t in 0 : #t` | `#t` |
| `#inc` | Term | `open #t in #raw : #t → #t` | `#t → #t` |
| `#get` | Term | `open #t in (\. $0) : #t → #Int` | `#t → #Int` |
| `#use` | Term | `#get (#inc (#inc #empty))` | `#Int` — composes sealed ops, never unfolds |

```
IntCounter.t → #t   IntCounter.empty → #empty   IntCounter.inc → #inc   IntCounter.get → #get
Math.inc → #raw     -- the raw Int→Int body, named and shared independently of the abstraction
```

A consumer composes the sealed ops and never unfolds; `#use` type-checks entirely over `#t`. Fine-grained dependency (criterion 4) holds with no record — `#use` depends on `#t`, `#inc`, `#get`, `#empty` and nothing else. The internal/external split survives unbundling: `#raw` is `Int → Int`, shared with every increment-on-`Int` and nameable as `Math.inc`; `#inc` is the distinct sealed wrapper of type `t → t`.

**The catch is enforcement, and it is a genuine trilemma.** For a freely-authorable `open #A in E : T` node, at most two of these hold at once:

1. **Flat & unbundled** — any author can submit a sealed op as its own independently-stored node.
2. **Pure-content revalidation** — ingest re-derives well-typedness from reachable bytes alone, no out-of-store secret.
3. **Enforced opacity** — a party cannot fabricate an unsanctioned op: `open #A in (0 - 5) : #A` mints a bogus abstract value; `open #A in (\. $0) : #A → witness` reads the representation back out.

The obstruction is structural. In a content-addressed store everything is reachable, so any in-store gate is public and forgeable; and the type cannot list its own sanctioned ops (`#A` would have to contain the op hashes that contain `#A` — a hash cycle), so the capability must flow op→type and live *outside* the bytes. Genuine correct-by-construction (form alone forbids forgery) is unavailable: the legitimate `open #A in 0 : #A` and a forged `open #A in (0 - 5) : #A` are equally witness-substitutable, and elimination cannot be made free or the representation leaks.

This trilemma also names the latent tension in §"Opacity is a typing discipline": the worked example treats sealed exports as independent nodes consumers depend on directly (`#B → #I`), yet that section calls them "not independently-ingestable" for forgery-safety. A node cannot be both. That is positions (1) and (3) held together while keeping (2) — which the trilemma forbids. The bundled design earns ingest-forgery-safety only by giving up (1): making the *structure* the trust-and-ingest unit, with seals genuine fields rather than standalone dependable hashes.

**Lean (2026-06-04): editor-enforced opacity, for a cooperative threat model.** Keep (1) and (2) — flat, unbundled, pure-content nodes — and locate opacity in the editing layer rather than at ingest:

- The editor only *offers* the `open #A` affordance while the author is working inside `#A`'s unsealing set (see the finding below) — a UI grouping derived by scanning the store, **not** a stored bundle and **not** the namespace hierarchy.
- A derived aspect can mark each `open #A` node as sanctioned, surfacing unsanctioned opens as warnings ("no silent breakage") rather than blocking ingest.

This defends against honest mistakes, not a party who hand-writes bytes — accepted, because the near-term commons is cooperative. **Deferred alternative:** to recover ingest-level / adversarial-safe enforcement *without* bundling, make the mint a keypair — its public half in `#A`'s hash (the distinctness mint doing double duty), its secret half an out-of-store capability, each op carrying a deterministic signature as an attestation aspect that ingest verifies. Cryptographic safety is explicitly not pursued now; it is recorded as the long-term path. See open-questions §"Type abstraction."

### The unsealing set is broader than the operations (p12 finding, 2026-06-05)

The natural derived query for an abstract type is **the set of definitions that unseal it** — every `open #A` / `Seal` node whose `opens` includes `#A`, found by scanning. p12 first called this the type's *implementation set* and equated it with the operations; that held only because the early examples had nothing else opening the type. Adding **internal tests** — boolean definitions that observe the representation, e.g. `Counter.incr Counter.empty == 1`, authored with `#A` open — broke the equation: they unseal `#A` (so they belong to the set) but are not operations.

The sharper point is that **the substrate draws no distinction at all.** An internal test is *structurally identical* to an operation — a `Seal` over the opened `#A`, stored / type-checked / evaluated by exactly the same machinery; its "test-ness" is only an `11`-style Attachment aspect tag plus interface-level evaluation of the boolean. So "operation" is not a substrate category: at best it is "an unsealing definition that isn't tagged a test," a heuristic. The honest notion is the **unsealing set** (p12 renames the query accordingly); an editor *presents* that set and may **categorize** it by aspect (test vs not) or by namespace, but the substrate itself only knows "unseals `#A`." This is also the natural place to read off *what is permitted to observe the representation* — an auditable question whose honest answer includes the tests, so they should not be filtered out. A principled operation-vs-internal split would require an explicit **export** marker — the deferred `public`/`private` story below; until that exists, the unsealing population is flat.

A smaller mechanical consequence: an internal definition that both *calls* a sealed op and *unseals its result* (the test above) cannot be normalized to a witness-only body — its abstractness comes from the called op's return type, not from an annotation to rewrite. So a seal's implementation must be allowed to type-check under the seal's own `opens` (it is internal to the seal, and may see through what the seal opens). That is the resolution of the "mixed impl" question and the mechanism that makes internal definitions expressible at all.

## The dependency model and its criteria

A hard requirement drives much of the encoding: **changing one field of a module must not force consumers that did not depend on that field to upgrade** — including for modules with abstract types. First-class use of a module is the accepted exception (depend on the whole, upgrade on any change).

The mechanism: **module projection is an edit-time convenience that inlines to a direct reference.** Writing `M.incr` in a consumer resolves, at edit time, to a direct hash reference to the specific sealed operation — *not* a stored "project `incr` from `M`" node. The consumer's stored form depends only on the specific definitions it touched (the sealed op and the abstract-type def), never on the enclosing module record. This is the Unison "depend on definitions, not namespaces" property, applied through modules; it is consistent with arbor's standing rule that names resolve to hashes at edit time and never appear in stored programs. First-class module use — passing `M` around as a value — is the opt-in path that depends on the whole module hash.

The design holds itself to these invariants:

1. **Definitional identity.** Same content hash ⇒ same meaning. Abstract-type identity is a content hash (over `opaque ++ mint ++ witness`), never a side channel.
2. **Distinctness.** Two independently-authored abstractions are distinct even with identical witnesses (the mint). Two references to the same abstract-type definition are the same type.
3. **Soundness under representation change.** Changing the witness changes `t`'s identity, so values built against old and new representations cannot be mixed. (This is *why* criterion 1 includes the witness.)
4. **Fine-grained dependency.** Editing field F changes the hash of F and the enclosing module record, and nothing else. A consumer that referenced only G at edit time depends on G directly and is unaffected by edits to F.
5. **Honest upgrade.** Editing field F *does* require consumers of F to upgrade. No shielding from changes you actually depend on.
6. **Representation change is the one unavoidable break.** Editing the witness *type* breaks every consumer holding a `t` value or using a changed operation, by ordinary dependency propagation reaching `t`. This is inherent to the immutable-codebase design (every dependent is new when a dependency changes); abstraction adds no special rule. It is not offered to be avoided — that would be ABI migration, a deferred non-goal (`00-overview.md`).
7. **First-class coupling is opt-in.** Using a module as a value makes the consumer depend on the whole module hash; any field edit forces upgrade. Accepted price of first-class use.
8. **External abstractness regardless of access path.** Whether obtained first-class or via inlined projection, a consumer cannot observe the witness; it appears in no definition the consumer depends on except behind the opaque tag in `t`'s own hash.
9. **Display round-trips.** An inlined module member renders via its module path `M.member` when that is the best available name; inlining does not lose the qualified display.

**A boundary worth stating honestly (criterion 6, sharpened).** The break triggers on a change to the witness *type* (list → array, `Int → Product` — the type-soundness cases). A change that keeps the same witness type but alters the representation *invariant* (store a count vs. its negation, both `Int`) does **not** move `t`'s hash and is **not** caught. That is the general fact that types do not capture invariants — true of any function edit, not special to abstraction — and within a single coherent checkout all operations come from the same version, so the invariant stays consistent; mixing only arises across checkouts / first-class use, where upgrade is already expected.

## Modules and the namespace

How module bindings interact with the hierarchical namespace (`04`) is **open**, but the current leaning is *away* from reifying namespace subtrees through the aspect machinery, and toward a namespace that is module-aware:

- **The namespace knows which entries denote modules.** A literal projection out of a known module (`Counter.incr`) inlines, at edit time, to a direct reference to the underlying definition (per the dependency model above) rather than being stored as a projection. On display, if the referenced definition has no other name, it renders via its module path (`Counter.incr`). This is the conservative, boundary-preserving option and the current preference.

Two other approaches are recorded as the spectrum:

- **Reify-on-demand (manual).** Namespace stays purely editing-layer; an explicit gesture reifies a subtree into a first-class module value (a `11` label-record) with its own hash. Cleanest boundary; most explicit.
- **First-class path-dependent access into namespaces (by hash).** Stored programs gain a reference form that projects a binding out of a reified module-of-a-subtree. Compatible with content-addressing as long as it references *hashes* (no names in canonical bytes), but it is the largest addition to the node encoding and most blurs "is the namespace part of the program." Recorded as the ambitious end, deferred.

A standing correction to an earlier worry: **path-dependent abstract types by hash** — `Project(module_or_mint_hash, label_hash)` — put **no names in canonical bytes** and do not violate the `04` editing-layer boundary. Only storing dotted *name* strings would, which the substrate never does.

## Worked example: a `Counter` module with sealing

Symbolic hashes. The store is global and content-addressed; the namespace is per-checkout (editing layer). The witness is `Int`; the abstract type is `t`.

**Signature** (a witness-free `Type` definition):

```
COUNTER = sig {
  t     : Type        -- opaque type-component
  empty : t
  incr  : t → t
  get   : t → Int
  decr  : t → t
}
```

**Implementation** (transparent structure, `t = Int` visible):

```
CounterImpl = struct {
  type t = Int
  empty = 0
  incr  = \x. x + 1
  get   = \x. x
  decr  = \x. x - 1
}
```

**Sealing** `Counter = CounterImpl :> COUNTER` mints `t`'s abstract-type definition and types the exported operations over it.

**Store (content-addressed, shared across checkouts).** Stored terms are name-free (de Bruijn; `$0` is the innermost bound variable); human names live only in the namespace. The raw bodies (`#zero`/`#i_raw`/`#g_raw`/`#d_raw`) and the sealed exports (`#E`/`#I`/`#G`/`#D`) are kept as separate nodes to show the internal/external split.

*Primitive type, labels, signature:*

| hash | sort | content | `Type_of` |
|------|------|---------|-----------|
| `#Int` | Type | `Int` (primitive) | — |
| `#Lt`/`#Le`/`#Li`/`#Lg`/`#Ld` | Label | `mint` each | display `t`/`empty`/`incr`/`get`/`decr` |
| `#SIG` | Type | `Sig[(#Lt, TypeComp⟨opaque⟩); (#Le, ⟨#Lt⟩); (#Li, ⟨#Lt⟩→⟨#Lt⟩); (#Lg, ⟨#Lt⟩→#Int); (#Ld, ⟨#Lt⟩→⟨#Lt⟩)]` — witness-free; `⟨#Lt⟩` = the sig-bound type-component | (is a type) |

*Abstract type (minted by the seal):*

| hash | sort | content | note |
|------|------|---------|------|
| `#T` | Type | `Abstract{ opaque; mint:m1; witness:#Int }` | displays `Counter.t`; `witness:#Int` is reachable but never unfolded outside the seal |

*Raw implementation bodies — internal types over `#Int`, generic and structurally shared:*

| hash | sort | content | `Type_of` |
|------|------|---------|-----------|
| `#zero` | Term | `0` | `Int` |
| `#i_raw` | Term | `\. $0 + 1` | `Int → Int` |
| `#g_raw` | Term | `\. $0` (the identity fn) | `Int → Int` |
| `#d_raw` | Term | `\. $0 - 1` | `Int → Int` |

*Sealed exports — external types over `#T`; the projection targets:*

| hash | sort | content | `Type_of` | depends on |
|------|------|---------|-----------|------------|
| `#E` | Term | `Seal{ ty:#T, impl:#zero }` | `#T` | `#T`, `#zero` |
| `#I` | Term | `Seal{ ty:#T→#T, impl:#i_raw }` | `#T → #T` | `#T`, `#i_raw` |
| `#G` | Term | `Seal{ ty:#T→#Int, impl:#g_raw }` | `#T → #Int` | `#T`, `#g_raw`, `#Int` |
| `#D` | Term | `Seal{ ty:#T→#T, impl:#d_raw }` | `#T → #T` | `#T`, `#d_raw` |

*Module record and consumers:*

| hash | sort | content | `Type_of` | depends on |
|------|------|---------|-----------|------------|
| `#M` | Term | `Module{ sig:#SIG; [(#Lt,#T),(#Le,#E),(#Li,#I),(#Lg,#G),(#Ld,#D)] }` | `#SIG` | all above |
| `#B` | Term | `\. #I (#I $0)` — `bump2` | `#T → #T` | **`#T`, `#I`** |
| `#R` | Term | `\. #G (#I $0)` — `readout` | `#T → #Int` | **`#T`, `#I`, `#G`** |

The internal/external split is the `#g_raw`/`#G` pair: `#g_raw = \. $0` has type `Int → Int` and is shared with every identity-on-`Int` in the store; `#G = Seal{#T→#Int, #g_raw}` is a *distinct* node of type `t → Int`. `Counter.get` is `#M`'s `#Lg` field → `#G`, so a consumer inlining `Counter.get` references `#G` (abstract type), never `#g_raw`. `#B` depends on `#T` and `#I` only — **not** `#M`, `#D`, `#G`, `#E`, or any raw body. The raw bodies are maximally shared; the abstraction lives entirely in the thin `Seal{…}` wrappers.

**Checkout `main`** (namespace = name → hash):

```
Counter → #M     Counter.t → #T     Counter.empty → #E     Counter.incr → #I
                 Counter.get → #G   Counter.decr → #D      bump2 → #B   readout → #R
```

**Branch `lenient-decr`** — edit only `decr` (clamp at zero). New sealed op `#D2`, hence new record `#M2 = {…, decr↦#D2}`. Everything else byte-identical:

```
Counter → #M2 (changed)   Counter.decr → #D2 (changed)   Counter.t → #T (same)
Counter.incr → #I (same)  bump2 → #B (same — untouched)
```

- `#B` is the *same bytes* in both checkouts: it touched neither `#T` nor `#I` (criterion 4). No upgrade; automatic sharing.
- Only code using `Counter.decr` directly, or using `Counter` first-class (`#M`), sees `#D2`/`#M2` (criteria 5, 7).
- `#B` renders in either checkout as `\c: Counter.t. Counter.incr (Counter.incr c)` — the stored term holds no names; each checkout's namespace reconstructs them (criterion 9 / the display story).

**Branch `pair-counter`** — change the representation *type* to `Product(Int, Int)` (track count + max-seen). Witness `#Int → #IntPair`, so `t` becomes `#T3 = opaque ++ mint:m1 ++ witness:#IntPair` — **same mint `m1`** (same abstraction's lineage), different witness. Every operation re-types (`#I3`, `#E3`, …), and `bump2` re-authors to `#B3 = \c:#T3. #I3 (#I3 c)`:

```
Counter.t → #T3 (changed: witness moved)   Counter.incr → #I3 (changed)
bump2 → #B3 (changed — propagated)
```

This is criterion 6: a representation-*type* change propagates to `bump2` by ordinary dependency flow — contrast the `decr` edit, which did not. The mint `m1` persists through it, so tooling can still show `pair-counter`'s `Counter` as the same abstraction lineage as `main`'s.

> **p12 finding (2026-06-04): the mint-persistence here is illustrative, not required.** This is a *cross-branch* lineage claim (`main` vs. `pair-counter`). Soundness does not need it — the witness is in the hash, so the new type is distinct from the old whether the mint is carried or fresh, and old/new values cannot mix either way (p12 asserts this with a fresh mint). Distinctness wants a fresh mint anyway. *Within a single checkout*, lineage is already carried by the namespace binding (`Counter.t → #T` then `→ #T3`), so a representation change is just a fresh authoring of `Counter.t` rebound to the same name — no mint-reuse, no edit-of gesture. Carrying the mint forward is load-bearing only across checkouts/branches, where the name is no longer an authoritative link — the collaboration phase. p12 therefore drops mint-reuse and leans **dies-with-hash** (see `10` §"Marks that survive content edits").

The store-level intricacy here is a *bet that the editing layer can hide it*: a user sees `Counter.t` and `Counter.incr`, never `#T`/`#I`, and never knows a projection was flattened. What editing/checkout UX makes multi-version abstract modules legible is explicitly an open question for a later prototype, not something this doc answers.

## Subtyping and signature matching

Width matching ("a module with extra fields satisfies a smaller signature") **largely evaporates** for non-first-class use: under projection inlining, a consumer already depends only on the slice it used, so extraneous fields were never in its hash — exactly the property wanted, without a subtyping judgment. A real relation is needed only for **first-class** signature matching, and there the leaning is **coercion** (matching produces a restricted module — a new hash projecting the subset) over a `<:` judgment, because coercion fits content-addressing and the "you only depend on what you use" principle. Whether genuine depth/variance subtyping is ever needed is left open.

## Public / private

`public`/`private` is wanted at the **surface-syntax layer** but likely **not represented in the AST**. A private component is an underlying definition the *exported* (ascribed) module record does not bind a label for; the surface keyword controls what enters the exported record. No `private` node marker is required. Whether a third visibility level is ever needed (visible to siblings but not outside) is left open.

## Relation to existing concepts

- **Minted identity** (`10`). Abstract types are a second concrete population minted by definition (after `11`'s labels). The mint provides distinctness where coincident witnesses would otherwise collapse, and the "mark survives edits" leaning from `10` is what lets `pair-counter`'s `t` be recognized as the same abstraction lineage as `main`'s across a representation change. **p12 refines this:** lineage recognition is the *only* thing mark-survival adds for abstract types — soundness rides witness-in-hash, distinctness wants a fresh mark, and within a single checkout lineage is carried by the namespace. So mark-survival is deferred to the cross-checkout/collaboration phase; p12 uses a fresh mint per authoring (dies-with-hash) and treats a representation change as a fresh `Counter.t` rebound to the same name.
- **Label sort** (`11`). A signature *is* a label-record; operation labels are minted, namespace-decoupled handles. This doc consumes `11`'s "intentional label sharing" thread directly: row-polymorphism / module-subtyping over records is the same shape one level up. Generic code references operation labels by hash — the precise reason content-addressing alone is insufficient (Purpose §).
- **Content addressing** (`03`). Strengthens the "hashing types as well as terms" thread: signatures and abstract types are first-class `Type` definitions. The witness-in-hash requirement is a new, sharp consequence of combining content addressing with soundness.
- **Naming layer** (`04`). Module projection is an edit-time name resolution that inlines to a hash reference; the boundary (no names in stored programs) is preserved. Path-dependent types by hash do not threaten it.
- **Language model** (`01`). The natural prototype home is the existentials region (Ch. 24), with functors/translucency edging toward F-omega (Ch. 29). Records (Ch. 11) are the prerequisite that `11` already targets.

## Non-goals (current phase)

- **No prototype prescription.** Far past the current frontier; matures in design first.
- **No ABI migration.** Representation change breaking consumers (criterion 6) is accepted, not worked around. Runtime coercion / value migration across hash changes stays the deferred non-goal it is in `00`.
- **No row-variable commitment.** Whether the type-language needs explicit row variables or structural matching over fixed label sets suffices is inherited-open from `11`.
- **No editing-interface design.** The model bets the editing layer can hide store-level intricacy; designing that layer is deliberately deferred (and noted below).
- **No law enforcement.** A monoid signature does not enforce monoid laws; as elsewhere, semantic invariants are not type-system guarantees.

## Open sub-questions

Tracked in `open-questions.md` under "Type abstraction."

- **Module ↔ namespace relationship.** Module-aware namespace with inlined projection (current lean) vs. reify-on-demand vs. first-class path-dependent access. The display/round-trip contract and what gesture distinguishes "use this module's member" from a plain named reference are open.
- **Open vs. closed existential scope.** Closed `unpack … in …` is too restrictive for sophisticated programs; the leaning is **path-dependent** access (`m.t` by hash), with OCaml-style unpack-into-a-binding and open-existential (Montagu–Rémy, with the attendant avoidance problem) as the alternatives. Not settled.
- **Signature/translucency encoding.** Exact node shape for translucent signatures (per-component opaque/manifest), and how value-component types reference type-component labels by hash within a self-referential record.
- **First-class signature matching.** Coercion (restricted-module, new hash) vs. a subtyping judgment; whether depth/variance is ever needed.
- **Functor application identity.** Applicative by default is the lean; the precise contract for editor-authored `F(X)` (mints) vs. computed `F(X)` (content-addressed) and the explicit-unit generative escape hatch needs pinning when a prototype forces it.
- **Same-witness-type invariant drift.** The criterion-6 boundary: changes that preserve the witness type but alter its invariant are uncaught. Whether anything beyond "general semantic drift, not abstraction-specific" needs saying.
- **Editing / checkout UX.** What makes multi-version abstract modules legible to a user who never sees hashes. For a later interface prototype.
- **Recursive modules.** Inherited from `11`; modules whose fields reference other definitions in the same module intersect the "Mutual recursion canonicalization" thread under "Content addressing."

## Connections

- `11-label-sort.md` — a signature is a label-record; operation labels are minted handles; the row-polymorphism/module-subtyping thread is the same shape one level up.
- `10-minted-identity.md` — the mint mark supplies abstraction distinctness and (under the marks-survive-edits lean) cross-version lineage.
- `03-content-addressing.md` §"Hashing types as well as terms" — signatures and abstract types as first-class `Type` definitions; witness-in-hash is the new consequence.
- `04-naming-layer.md` — projection inlining is edit-time name resolution; names stay out of stored programs.
- `01-language-model.md:94` — existentials (Ch. 24) / F-omega (Ch. 29) as the prototype region.
- `00-overview.md` §"Non-goals" — ABI migration stays deferred; criterion 6 is the honest consequence.
