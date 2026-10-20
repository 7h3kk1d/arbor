# Type Abstraction

**Status:** Draft. The substrate direction below is a set of leanings from design discussion, not commitments. When written, no prototype had exercised modules, signatures or existentials, and the expected trigger was the TAPL Ch. 24 (existentials) / Ch. 29 (F-omega) region. Since then the type-abstraction line has exercised all three: p12 (abstract types), p13 (System-F), p14–p15 (existentials), p16 (records), p17 (translucent modules).

## Purpose

The substrate needs type abstraction: logic written generically over types whose representation is hidden. Two cases motivate it.

- **Generic algorithms over an interface.** An algorithm over stacks should not care whether the stack is an array or a linked list, only that it provides `push`/`pop`/`empty` at the right types. This is the functor shape. Code is parameterized over an implementation, the carrier type is abstract inside the body, and the caller chooses the implementation.
- **Algebraic structures.** A monoid is a carrier `A` with `id : A` and `op : A → A → A`. Code over any monoid must obtain the operations, including `id`, a value of the abstract carrier, without already holding a value of `A`. This is the existential package shape, `∃A. { id : A, op : A → A → A }`. Unpacking the package yields `id` directly.

These are two quantifiers (`∀` for the functor, `∃` for the package) over one interface shape. That shape connects to the rest of arbor: a signature is an existential or universal binder around a `11-label-sort.md` label-record.

### Why content-addressing alone is not enough

A hash is already an opaque handle, so content addressing can look like abstraction. It is not, for two reasons that recur below.

1. **Generic code cannot name the concrete operation by hash.** An algorithm over any stack must reference `push` before any particular stack exists. A concrete `push`'s hash varies per implementation, so the algorithm references the label `push` (a label hash, per `11`) and the implementation supplies the binding at use. Content addressing identifies the `push`; abstraction needs a `push`-shaped slot.
2. **Hiding a representation is a typing judgment, not a storage fact.** A value's `Int` representation is visible in its type unless the type system refuses to expose it. Sealing performs that refusal, and it lives in the type system.

## The two quantifiers, concretely

One label-record signature carries both readings:

```
StackOps R = { empty : R, push : Int → R → R, pop : R → Option (Int × R) }
```

- **Functor (`∀`, genericity over implementations).** A stack algorithm is a functor `∀R. StackOps R → Client`. The implementation is supplied now or by a caller, and `R` is abstract in the body. This is the primary genericity story.
- **Existential package (`∃`, hiding a representation).** A first-class stack value has type `∃R. StackOps R`. A function receiving one `unpack`s it and cannot observe `R`. The monoid `∃A. { id : A, op : A → A → A }` has the same shape, and unpacking yields `id` without a value of `A`.

Only the outer binder differs. Signature = binder + label-record.

## Abstract-type identity

The central question is what an abstract type's identity is under content addressing. The current leaning:

> **An abstract type is a content-addressed `Type` definition whose content is `opaque_tag ++ mint_mark ++ witness_ref`.**

Each ingredient does necessary work.

- **Witness in the hash, for soundness.** If the representation changes, the abstract type's identity must change, or values built against the old and new representations can be mixed. Seal a stack over a linked list (`push_v1 : Int → t → t`), then change the representation to an array (`push_v2 : Int → t → t`). If `t`'s identity survived the edit, `push_v2 3 empty_v1` would typecheck and apply an array operation to a list value. So the witness is inside `t`'s hash. This keeps identity pure: same content hash means same meaning, and abstract-type identity is a content hash, not a side channel.
- **Mint mark, for distinctness.** Without it, two independently authored abstractions with the same witness collapse. `Counter` over `Int` and `Celsius` over `Int` would both encode as `hash(opaque_tag ++ Int)`, and a temperature would typecheck as a counter. The mint mark (per `10-minted-identity.md`) separates them. Every authoring gesture draws a fresh mark, so independent abstractions never share a mint, and a mark persists only down one definition's edit lineage. Content alone fails only when witnesses coincide, and that is the case the mint covers.
- **Opaque tag, for hiding.** External typechecking refuses to unfold `t` to its witness. Only the defining module's own check unfolds it, to verify the operations.

This makes abstract types a concrete population minted by definition, after `11`'s labels. It partly answers `10`'s per-definition vs. per-language opt-in question by example: modules bearing abstract types mint on authoring.

### Generative vs. applicative, mapped onto authoring vs. computation

The ML axis of SML generative sealing vs. OCaml applicative functors maps onto arbor's authoring/computation distinction.

- **Structure sealing is generative by authoring.** Authoring a module with an abstract type draws a fresh mint. Two independently authored modules with identical implementations are distinct. Identities coincide only for the same definition or an edit-descendant that carries its mint forward.
- **Functor application is applicative by default.** Applying a functor is a computation, so no mint is drawn. The result is content-addressed by `(functor_hash, argument_hash)`, so `F(X)` always yields the same module hash and `F(X).t = F(X).t` holds for free. Generative application would require the functor to mint on each call, which conflicts with pure computation. Authoring a fresh `F(X)` in the editor is an authoring gesture and does mint. An explicit unit parameter (`F() :> SIG`, OCaml-style) forces generative application on demand.

## Sealing and translucency

Sealing lives in the type system. In surface→internal→node terms:

- **Signatures are content-addressed `Type` definitions.** A signature is a `11`-style label-record of components. A type-component declares a label of kind `Type`, either opaque (`type t`) or manifest (`type t = Int`); this is the translucent-sum design of Harper–Lillibridge and Leroy. A value-component declares a label whose type may reference type-component labels by hash. A signature is the interface, so it carries no witness. Its hash contains label hashes and type structure and no names (per `04`).
- **A structure is the implementation.** It is a label-record binding the same labels to concrete types (`t = Int`) and terms. The witness is visible.
- **Ascription `M :> S` seals.** It checks `M` against `S`, mints a fresh abstract-type definition (`opaque ++ mint ++ witness`) for each opaque type-component of `S`, and produces a sealed module whose exported operations are typed over those abstract types. Manifest components keep their equations, since `t = Int` is part of the signature's content. Translucency is per-component: a signature can expose some types and hide others.
- **Node encoding.** Exported operations are sealed wrappers, distinct `Seal{ ty, impl }` nodes. `impl` is the raw implementation, a term typed over the witness and structurally shared like any other. `ty` is the external type over the abstract-type hash. The wrapper's `Type_of` is `ty`; the raw body's `Int → Int`-style type stays nested. A sealed export therefore hashes differently from its raw body and from every other term sharing that body. The witness↔abstract-type correspondence is known only inside the module's own typecheck and is never exported.
- **Pretty-printing.** Abstract types and sealed operations reverse-look-up through the namespace and render by name (`Counter.t`, `Counter.incr`), with short hashes when unbound, as in `11` and p9.

Ascription subsumes `public`/`private` and information hiding (see §"Public / private").

### Opacity is a typing discipline, not secrecy

Everything in a content-addressed store is reachable. A client can follow an abstract type's `witness_ref` and read a sealed operation's raw body. Opacity does not depend on hiding those bytes. It is a property of the typing rules: the checker grants `t ≡ witness` to no one who merely references the abstract-type hash. The witness is in the hash for identity and soundness, and it is consulted for unfolding in one place only, inside the structure that creates the abstract type.

The lifecycle that makes this forgery-safe:

1. **Author a structure.** `struct { type t = W; … } :> SIG` is checked with `type t = W` transparent and local. Here `t` is a local binder, not yet the abstract-type hash, and the operations typecheck against the witness.
2. **Seal.** The seal mints `#A = opaque ++ mint ++ witness:W`, substitutes `t ↦ #A` in the operations' types, and emits the sealed-export nodes (now typed over `#A`), the module record, and `#A` itself. These outputs are trusted because they were just checked against the transparent witness.
3. **From then on, `#A` is opaque.** No definition that references `#A` may unfold it. Sealed exports carry their external types as data established once, not re-derived against `#A`'s witness on use.

Two consequences establish safety:

- **Sealed exports are outputs of a validated structure, not independently ingestable nodes.** A hand-written `Seal{ #A → #A, <anything> }` fails ingest, because ingest does not unfold `#A` for the forger and `<anything> : W → W` does not check against the opaque `#A → #A`.
- **Re-authoring the structure mints a fresh abstract type.** Authoring `struct { type t = W; … } :> SIG` again produces `#A' ≠ #A`. A forger gets their own abstraction, never values of the original `#A`. The only route to an `#A` value is the original module's sealed operations.

This is ML generativity in content-addressed terms. Witness-in-hash (soundness) and opacity (safety) do not conflict because they act at different moments for different parties: the witness is unfolded once, locally, at sealing, and opacity holds everywhere else.

## Unbundled abstract types and the opacity trilemma

*Design exploration, 2026-06-04. This section considers one abstract type and its operations in isolation, with no functor and no first-class module value. Functors and first-class modules are covered by the dependency-model and subtyping sections.*

In isolation the `Module{}` record is unnecessary. Three independent pieces carry the abstraction, and naming is separate:

- **One `Type` node** `opaque(mint, witness)`, with the mint for distinctness and the witness in the hash for soundness, as above.
- **Independent sealed-wrapper nodes per operation**, `open #A in E : T`. Each is its own definition. `E` is the raw implementation typed over the witness and `T` is the external type over `#A`. `open` is the local, controlled unfold: inside it the checker treats `#A ≡ witness`, and the ascription `: T` re-seals at the boundary. This is the worked example's `Seal{ ty, impl }` node with the unfold made explicit.
- **A separate name → hash table** with no bearing on type identity.

| hash | sort | content | external `Type_of` |
|------|------|---------|--------------------|
| `#t` | Type | `opaque(mint:m1, witness:#Int)` | (is a type; displays `IntCounter.t`) |
| `#raw` | Term | `\. $0 + 1` | `Int → Int` (raw body, freely shared) |
| `#empty` | Term | `open #t in 0 : #t` | `#t` |
| `#inc` | Term | `open #t in #raw : #t → #t` | `#t → #t` |
| `#get` | Term | `open #t in (\. $0) : #t → #Int` | `#t → #Int` |
| `#use` | Term | `#get (#inc (#inc #empty))` | `#Int`; composes sealed ops, never unfolds |

```
IntCounter.t → #t   IntCounter.empty → #empty   IntCounter.inc → #inc   IntCounter.get → #get
Math.inc → #raw     -- the raw Int→Int body, named and shared independently of the abstraction
```

`#use` composes the sealed ops and typechecks entirely over `#t`. Fine-grained dependency (criterion 4) holds without a record: `#use` depends on `#t`, `#inc`, `#get` and `#empty` only. The internal/external split survives unbundling. `#raw` is `Int → Int`, shared with every increment on `Int` and nameable as `Math.inc`, while `#inc` is the distinct sealed wrapper of type `t → t`.

**Enforcement is a trilemma.** For a freely authorable `open #A in E : T` node, at most two of these hold:

1. **Flat & unbundled.** Any author can submit a sealed op as its own stored node.
2. **Pure-content revalidation.** Ingest re-derives well-typedness from reachable bytes alone, with no out-of-store secret.
3. **Enforced opacity.** No party can fabricate an unsanctioned op. `open #A in (0 - 5) : #A` mints a bogus abstract value; `open #A in (\. $0) : #A → witness` reads the representation back out.

The obstruction is structural. Everything in the store is reachable, so any in-store gate is public and forgeable. The type cannot list its own sanctioned ops, since `#A` would have to contain op hashes that contain `#A`, a hash cycle. The capability must therefore flow from op to type and live outside the bytes. Correct-by-construction is unavailable: the legitimate `open #A in 0 : #A` and the forged `open #A in (0 - 5) : #A` are equally witness-substitutable, and elimination cannot be made free without leaking the representation.

The trilemma also exposes a tension in §"Opacity is a typing discipline, not secrecy". The worked example treats sealed exports as independent nodes that consumers depend on directly (`#B → #I`), while that section calls them not independently ingestable. Holding both is positions (1) and (3) together with (2), which the trilemma rules out. The bundled design gets ingest-level forgery safety only by giving up (1): the structure becomes the unit of trust and ingest, and seals are fields of it, not standalone hashes others depend on.

**Lean (2026-06-04): editor-enforced opacity, for a cooperative threat model.** Keep (1) and (2), flat unbundled pure-content nodes, and locate opacity in the editing layer instead of at ingest:

- The editor offers the `open #A` affordance only while the author works inside `#A`'s unsealing set (see the next subsection). That set is a UI grouping derived by scanning the store. It is neither a stored bundle nor the namespace hierarchy.
- A derived aspect can mark each `open #A` node as sanctioned and surface unsanctioned opens as warnings ("no silent breakage") without blocking ingest.

This defends against honest mistakes, not against a party who hand-writes bytes. That is acceptable because the near-term commons is cooperative. **Deferred alternative:** adversarial-safe enforcement at ingest without bundling needs the mint to be a keypair. Its public half sits in `#A`'s hash, so the distinctness mint does double duty. Its secret half is an out-of-store capability, and each op carries a deterministic signature as an attestation aspect that ingest verifies. Cryptographic safety is not pursued now and is recorded as the long-term path; see `open-questions.md` §"Type abstraction".

### The unsealing set is broader than the operations (p12 finding, 2026-06-05)

The natural derived query for an abstract type is the set of definitions that unseal it: every `open #A` / `Seal` node whose `opens` includes `#A`, found by scanning. p12 first called this the type's implementation set and equated it with the operations. That held only because nothing else in the early examples opened the type. Internal tests broke it. A test such as `Counter.incr Counter.empty == 1`, authored with `#A` open, unseals `#A` but is not an operation.

The substrate draws no distinction between the two. An internal test is structurally identical to an operation: a `Seal` over the opened `#A`, stored, typechecked and evaluated by the same machinery. Its test status is only an `11`-style Attachment aspect tag plus interface-level evaluation of the boolean. "Operation" is therefore not a substrate category; at best it means an unsealing definition not tagged as a test, which is a heuristic. p12 renames the query to the unsealing set. An editor presents that set and may group it by aspect (test or not) or by namespace, but the substrate only knows "unseals `#A`". The set also answers which definitions may observe the representation. That question is auditable, its answer includes the tests, and they should not be filtered out. A principled split between operations and internals needs an explicit export marker, the deferred `public`/`private` story below. Until then the unsealing population is flat.

A mechanical consequence: a definition that both calls a sealed op and unseals the result, like the test above, cannot be normalized to a witness-only body. Its abstractness comes from the called op's return type, not from an annotation that could be rewritten. A seal's implementation must therefore typecheck under the seal's own `opens`, since it is internal to the seal and may see through what the seal opens. This resolves the "mixed impl" question and is what makes internal definitions expressible.

## The dependency model and its criteria

One requirement drives much of the encoding: changing one field of a module must not force consumers that did not depend on that field to upgrade, including for modules with abstract types. First-class use of a module is the accepted exception; it depends on the whole and upgrades on any change.

The mechanism is that module projection is an edit-time convenience that inlines to a direct reference. `M.incr` in a consumer resolves at edit time to a hash reference to the specific sealed operation, not to a stored "project `incr` from `M`" node. The consumer's stored form depends only on the definitions it touched (the sealed op and the abstract-type definition), never on the enclosing module record. This is Unison's "depend on definitions, not namespaces" applied through modules, and it matches arbor's rule that names resolve to hashes at edit time and never appear in stored programs. Passing `M` around as a value is the opt-in path that depends on the whole module hash.

The design holds itself to these criteria:

1. **Definitional identity.** Same content hash means same meaning. Abstract-type identity is a content hash over `opaque ++ mint ++ witness`, never a side channel.
2. **Distinctness.** Two independently authored abstractions are distinct even with identical witnesses, because of the mint. Two references to the same abstract-type definition are the same type.
3. **Soundness under representation change.** Changing the witness changes `t`'s identity, so values built against old and new representations cannot be mixed. This is why criterion 1 includes the witness.
4. **Fine-grained dependency.** Editing field F changes the hash of F and of the enclosing module record, nothing else. A consumer that referenced only G at edit time depends on G directly and is unaffected.
5. **Honest upgrade.** Editing F does require consumers of F to upgrade.
6. **Representation change is the one unavoidable break.** Changing the witness type breaks every consumer holding a `t` value or using a changed operation, through ordinary dependency propagation from `t`. This follows from the immutable-codebase design, where every dependent is new when a dependency changes, and abstraction adds no special rule. Avoiding it would be ABI migration, a deferred non-goal (`00-overview.md`).
7. **First-class coupling is opt-in.** Using a module as a value makes the consumer depend on the whole module hash, so any field edit forces an upgrade.
8. **External abstractness regardless of access path.** Whether a consumer obtains the module first-class or by inlined projection, it cannot observe the witness. The witness appears in no definition the consumer depends on except behind the opaque tag in `t`'s own hash.
9. **Display round-trips.** An inlined module member renders as `M.member` when that is the best available name.

**The boundary of criterion 6.** The break triggers on a change to the witness type (list → array, `Int → Product`), which are the type-soundness cases. A change that keeps the witness type but alters the representation invariant, such as storing a count vs. its negation (both `Int`), does not move `t`'s hash and is not caught. Types do not capture invariants; this holds for any function edit and is not specific to abstraction. Within one coherent checkout all operations come from the same version, so the invariant stays consistent. Mixing arises only across checkouts or through first-class use, where upgrade is already expected.

## Modules and the namespace

How module bindings interact with the hierarchical namespace (`04`) is open. The current leaning is against reifying namespace subtrees through the aspect machinery and toward a module-aware namespace:

- **The namespace knows which entries denote modules.** A literal projection out of a known module (`Counter.incr`) inlines at edit time to a direct reference to the underlying definition, per the dependency model, and is not stored as a projection. On display, a referenced definition with no other name renders via its module path (`Counter.incr`). This is the conservative option that preserves the boundary, and it is the current preference.

Two other points on the spectrum:

- **Reify on demand (manual).** The namespace stays purely editing-layer, and an explicit gesture reifies a subtree into a first-class module value (a `11` label-record) with its own hash. This has the cleanest boundary and is the most explicit.
- **First-class path-dependent access into namespaces, by hash.** Stored programs gain a reference form that projects a binding out of a reified module-of-a-subtree. This is compatible with content addressing as long as it references hashes, with no names in canonical bytes. It is the largest addition to the node encoding and most blurs whether the namespace is part of the program. Recorded as the ambitious end and deferred.

A correction to an earlier worry: path-dependent abstract types by hash, `Project(module_or_mint_hash, label_hash)`, put no names in canonical bytes and do not violate the `04` editing-layer boundary. Only storing dotted name strings would, and the substrate never does.

## Worked example: a `Counter` module with sealing

Hashes are symbolic. The store is global and content-addressed; the namespace is per-checkout (editing layer). The witness is `Int` and the abstract type is `t`.

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

**Store (content-addressed, shared across checkouts).** Stored terms are name-free (de Bruijn; `$0` is the innermost bound variable), and names live only in the namespace. Raw bodies (`#zero`/`#i_raw`/`#g_raw`/`#d_raw`) and sealed exports (`#E`/`#I`/`#G`/`#D`) are separate nodes, to show the internal/external split.

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

*Raw implementation bodies, with internal types over `#Int`, generic and structurally shared:*

| hash | sort | content | `Type_of` |
|------|------|---------|-----------|
| `#zero` | Term | `0` | `Int` |
| `#i_raw` | Term | `\. $0 + 1` | `Int → Int` |
| `#g_raw` | Term | `\. $0` (the identity fn) | `Int → Int` |
| `#d_raw` | Term | `\. $0 - 1` | `Int → Int` |

*Sealed exports, with external types over `#T`; these are the projection targets:*

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
| `#B` | Term | `\. #I (#I $0)` — `bump2` | `#T → #T` | `#T`, `#I` |
| `#R` | Term | `\. #G (#I $0)` — `readout` | `#T → #Int` | `#T`, `#I`, `#G` |

The `#g_raw`/`#G` pair shows the internal/external split. `#g_raw = \. $0` has type `Int → Int` and is shared with every identity on `Int` in the store. `#G = Seal{#T→#Int, #g_raw}` is a distinct node of type `t → Int`. `Counter.get` is `#M`'s `#Lg` field, `#G`, so a consumer inlining `Counter.get` references `#G` and never `#g_raw`. `#B` depends on `#T` and `#I` only, not on `#M`, `#D`, `#G`, `#E` or any raw body. The raw bodies are maximally shared, and the abstraction lives entirely in the thin `Seal{…}` wrappers.

**Checkout `main`** (namespace = name → hash):

```
Counter → #M     Counter.t → #T     Counter.empty → #E     Counter.incr → #I
                 Counter.get → #G   Counter.decr → #D      bump2 → #B   readout → #R
```

**Branch `lenient-decr`** edits only `decr` to clamp at zero. That yields a new sealed op `#D2` and a new record `#M2 = {…, decr↦#D2}`. Everything else is byte-identical:

```
Counter → #M2 (changed)   Counter.decr → #D2 (changed)   Counter.t → #T (same)
Counter.incr → #I (same)  bump2 → #B (same — untouched)
```

- `#B` is the same bytes in both checkouts because it touched neither `#T` nor `#I` (criterion 4). No upgrade is needed and sharing is automatic.
- Only code using `Counter.decr` directly, or using `Counter` first-class (`#M`), sees `#D2`/`#M2` (criteria 5, 7).
- `#B` renders in either checkout as `\c: Counter.t. Counter.incr (Counter.incr c)`. The stored term holds no names, and each checkout's namespace reconstructs them (criterion 9).

**Branch `pair-counter`** changes the representation type to `Product(Int, Int)` to track count and max-seen. The witness moves from `#Int` to `#IntPair`, so `t` becomes `#T3 = opaque ++ mint:m1 ++ witness:#IntPair`: the same mint `m1` (same abstraction lineage) with a different witness. Every operation re-types (`#I3`, `#E3`, …), and `bump2` is re-authored as `#B3 = \c:#T3. #I3 (#I3 c)`:

```
Counter.t → #T3 (changed: witness moved)   Counter.incr → #I3 (changed)
bump2 → #B3 (changed — propagated)
```

This is criterion 6. A representation-type change propagates to `bump2` by ordinary dependency flow, where the `decr` edit did not. Because `m1` persists, tooling can show `pair-counter`'s `Counter` as the same abstraction lineage as `main`'s.

> **p12 finding (2026-06-04): mint persistence here is illustrative, not required.** This is a cross-branch lineage claim (`main` vs. `pair-counter`). Soundness does not need it: the witness is in the hash, so the new type is distinct from the old whether the mint is carried or fresh, and old and new values cannot mix either way (p12 asserts this with a fresh mint). Distinctness prefers a fresh mint anyway. Within one checkout the namespace binding already carries lineage (`Counter.t → #T`, then `→ #T3`), so a representation change is a fresh authoring of `Counter.t` rebound to the same name, with no mint reuse and no edit-of gesture. Carrying the mint forward matters only across checkouts and branches, where the name is no longer an authoritative link, which is the collaboration phase. p12 therefore drops mint reuse and leans dies-with-hash (see `10` §"Marks that survive content edits").

This store-level intricacy is a bet that the editing layer can hide it. A user sees `Counter.t` and `Counter.incr`, never `#T`/`#I`, and never learns that a projection was flattened. Which editing and checkout UX makes multi-version abstract modules legible is an open question for a later prototype.

## Subtyping and signature matching

Width matching (a module with extra fields satisfies a smaller signature) is mostly unnecessary for non-first-class use. Under projection inlining a consumer depends only on the slice it used, so extra fields were never in its hash, and no subtyping judgment is needed. A real relation is needed only for first-class signature matching. There the leaning is coercion, where matching produces a restricted module with a new hash projecting the subset, over a `<:` judgment, because coercion fits content addressing and "you only depend on what you use". Whether depth or variance subtyping is ever needed is open.

## Public / private

`public`/`private` is wanted in surface syntax but probably not in the AST. A private component is an underlying definition for which the exported (ascribed) module record binds no label. The surface keyword controls what enters the exported record, so no `private` node marker is needed, and ascription subsumes information hiding. Whether a third visibility level (visible to siblings, not outside) is ever needed is open.

## Relation to existing concepts

- **Minted identity** (`10`). Abstract types are the second population minted by definition, after `11`'s labels. The mint gives distinctness where coincident witnesses would collapse. `10`'s "mark survives edits" leaning is what would let `pair-counter`'s `t` be recognized as the same lineage as `main`'s. p12 narrows this: lineage recognition is the only thing mark survival adds for abstract types, since soundness rides on witness-in-hash, distinctness wants a fresh mark, and within one checkout the namespace carries lineage. Mark survival is deferred to the cross-checkout collaboration phase, and p12 mints fresh per authoring (dies-with-hash).
- **Label sort** (`11`). A signature is a label-record, and operation labels are minted handles decoupled from the namespace. This doc picks up `11`'s "intentional label sharing" thread: row polymorphism and module subtyping over records are the same shape one level up. Generic code references operation labels by hash, which is why content addressing alone is insufficient (§"Why content-addressing alone is not enough").
- **Content addressing** (`03`). Signatures and abstract types are first-class `Type` definitions, strengthening the "hashing types as well as terms" thread. Witness-in-hash is a new consequence of combining content addressing with soundness.
- **Naming layer** (`04`). Module projection is edit-time name resolution that inlines to a hash reference, so no names enter stored programs. Path-dependent types by hash do not threaten this.
- **Language model** (`01`). The natural prototype home is the existentials region (Ch. 24), with functors and translucency reaching toward F-omega (Ch. 29). Records (Ch. 11) are the prerequisite `11` already targets.

## Non-goals (current phase)

- **No prototype prescription.** The topic is far past the current frontier and matures in design first.
- **No ABI migration.** Representation change breaking consumers (criterion 6) is accepted. Runtime coercion and value migration across hash changes remain the deferred non-goal from `00`.
- **No row-variable commitment.** Whether the type language needs explicit row variables, or structural matching over fixed label sets suffices, is inherited open from `11`.
- **No editing-interface design.** The model bets the editing layer can hide store-level intricacy; designing that layer is deferred.
- **No law enforcement.** A monoid signature does not enforce the monoid laws. Semantic invariants are not type-system guarantees.

## Open sub-questions

Tracked in `open-questions.md` under "Type abstraction."

- **Module ↔ namespace relationship.** Module-aware namespace with inlined projection (current lean) vs. reify on demand vs. first-class path-dependent access. Open: the display round-trip contract, and what gesture distinguishes "use this module's member" from a plain named reference.
- **Open vs. closed existential scope.** Closed `unpack … in …` is too restrictive for larger programs. The leaning is path-dependent access (`m.t` by hash). Alternatives are OCaml-style unpack into a binding and open existentials (Montagu–Rémy, with the avoidance problem).
- **Signature/translucency encoding.** The node shape for translucent signatures (per-component opaque/manifest), and how value-component types reference type-component labels by hash within a self-referential record.
- **First-class signature matching.** Coercion to a restricted module with a new hash vs. a subtyping judgment, and whether depth/variance is ever needed.
- **Functor application identity.** Applicative by default is the lean. The contract for editor-authored `F(X)` (mints) vs. computed `F(X)` (content-addressed), and the explicit-unit generative escape hatch, need pinning when a prototype forces them.
- **Same-witness-type invariant drift.** Changes that keep the witness type but alter its invariant go uncaught (criterion 6). Open whether anything beyond "general semantic drift" needs saying.
- **Editing / checkout UX.** What makes multi-version abstract modules legible to a user who never sees hashes. For a later interface prototype.
- **Recursive modules.** Inherited from `11`. Modules whose fields reference other definitions in the same module meet the "Mutual recursion canonicalization" thread under "Content addressing."

## Connections

- `11-label-sort.md`: a signature is a label-record; operation labels are minted handles; row polymorphism and module subtyping are the same shape one level up.
- `10-minted-identity.md`: the mint mark supplies abstraction distinctness and, under the marks-survive-edits lean, cross-version lineage.
- `03-content-addressing.md` §"Hashing types as well as terms": signatures and abstract types as first-class `Type` definitions; witness-in-hash is the new consequence.
- `04-naming-layer.md`: projection inlining is edit-time name resolution; names stay out of stored programs.
- `01-language-model.md:94`: existentials (Ch. 24) and F-omega (Ch. 29) as the prototype region.
- `00-overview.md` §"Non-goals": ABI migration stays deferred; criterion 6 is the consequence.
