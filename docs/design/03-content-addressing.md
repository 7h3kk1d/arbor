# Content Addressing

**Status:** Draft.

## Purpose

Pin down how definitions are identified, how they reference each other, and how their hashes are grounded at the leaves. This doc covers only the hashing substrate. Caching of derived associated data is in `02-definitions-and-derived-data.md`; translation-specific caching is in `05-translation.md`.

## Why content addressing (added 2026-08-03)

This doc previously documented *how* we hash without ever arguing *why* — a gap worth closing, since minting (`10-minted-identity.md`) makes "issue a fresh key for every definition and keep a `key → data` map" a live alternative that is simpler, never collides, and solves lineage for free.

Content addressing buys four things a minted-key map cannot, all consequences of one property — **the identity is derivable by anyone, from the thing itself, without asking permission**:

- **Verification without trust.** A recipient can recompute the hash and check it, so the *store* need not be trusted: any mirror, peer, or agent can serve a definition. A `key → data` map requires trusting whoever holds the map.
- **Agreement without coordination.** Two parties who independently write the same definition arrive at the same name having never communicated; under minting they get different names and can never discover the match. Worth stating precisely because it is easy to misattribute to immutability: an append-only minted store is fully immutable and does not converge at all.
- **Cache keys that are theorems.** Same hash implies same content implies same derived result, which is what makes the eval-stability property in `02-definitions-and-derived-data.md` provable rather than engineered. Under minting, "these two keys denote equal content" is a separate fact someone must establish and maintain.
- **Immutability as arithmetic, not policy.** Content cannot change without the name changing. An append-only discipline over minted keys gets the same effect, but a discipline can be violated where hashing cannot.

Deduplication is the benefit most often cited and the least load-bearing for us — decisive at archive scale, near-irrelevant at ours.

**The converse is equally real, and is why `10-minted-identity.md` exists.** Every property above is about *sameness*, none about *identity through time*. Stable identity across edits, deliberate distinctness of coincidentally-equal definitions, and "this moved" are all things content addressing cannot express and minting gives away. So the two mechanisms answer different questions and a serious store needs both layers — as Git, Nix, IPFS, Perkeep, and Software Heritage all do. Our unusual choice is *placement*: the mint sits inside the hashed bytes, in the same node as semantic content, where Perkeep uses separate content-free identity blobs and Git keeps refs outside the object store entirely. Tracked in `open-questions.md` §"Minted identity".

The hard limit is **self-reference**: content addressing cannot name a thing that refers to itself, since the name depends on content that depends on the name. That is exactly the "Mutual recursion canonicalization" sub-question below, and it is not a local wrinkle — Nix meets the same wall with self-referential store paths and concedes its hash-rewriting workaround is only a heuristic. What is available to us and not to Nix is that our contents are **structured syntax rather than opaque bytes**, which admits more than one way out: make the recursive group one addressable unit with intra-group references by index (Unison's), or close the cycle with a binder in the object language and project the members out (a fixpoint over a product, positional or labeled). All three are worked through in §"Mutual recursion: three ways to close the cycle" below; none is chosen.

Sources and the fuller argument: `../related-work/01-content-addressing.md` §"Why content-address at all, rather than mint everything?".

## Identity

A definition is identified by its hash alone. We treat cryptographic hash collisions as impossible in practice.

- **Hash → definition** is a globally unique lookup. Given a hash, we return the full definition, which carries its language as a property.
- **Language is associated data on the definition,** not a component of identity. (This is a modeling convenience; it does not weaken the "no cross-language references" rule — that rule is enforced as a construction-time invariant, below.)
- There is one global hash-space. We do not partition by language.

## Hash input: canonicalized AST

The hash is over a *canonicalized* form of the definition's AST. Each language declares its canonicalizer. At minimum every language should collapse **α-equivalence** so that `λx.x` and `λy.y` hash identically.

Canonicalization is **syntactic by default.** Experimenting with coarser equivalences — β- or η-normalization, normal-form-up-to-reduction, extensional equality — is a per-language opt-in we haven't exercised. The door is intentionally open: a language may declare a more aggressive canonicalizer if that's the experiment it wants to run. Note that coarser equivalences discard distinctions that matter for resource behavior and for the programmer's intent, so the choice is not free.

Other per-language canonicalizations a language might declare:

- Normalizing record or variant field order, if the language's semantics treat them as unordered. (See `11-label-sort.md` for the substrate's preferred treatment of field labels — once labels are content-addressed hashes rather than strings, sort-by-label-hash becomes a natural canonical order.)
- Canonicalizing the presentation order of mutually recursive members (when we add recursion).
- Normalizing type-variable names in languages with explicit type binders.

## References in stored programs

Every cross-definition reference inside a stored program is a hash reference — `Ref(hash)` — never a name.

- **Names never appear in stored programs.** Name resolution happens at edit time and produces a hash reference; the stored artifact carries only the hash.
- **No cross-language references.** Per `decisions.md`, a definition in language L1 cannot carry a reference to a definition in language L2. The substrate enforces this as a construction-time invariant: when a definition is registered, every `Ref(hash)` in its body must resolve to a definition whose language matches the referring language. References that would span languages are rejected. Reuse across languages flows through translation.
- **Callers pin automatically.** Editing a definition produces a new hash; existing callers still reference the old hash. There is no silent migration. Upgrading callers is the editing layer's job — not the substrate's. See `04-naming-layer.md`.
- **Hashes ground transitively.** Definition A's hash depends on the hashes of definitions A references, which depend on theirs, and so on, bottoming out at primitives.

## Primitive identity

Primitives (language builtins with no body the system can hash over) ground the whole hash chain.

Following the procedure-identity decision from `02-definitions-and-derived-data.md`, primitives use the same model: **manually-versioned tags.**

- A primitive is identified by `(namespaced-tag, version, signature)`, e.g., `int:add:v1` with a declared signature in its language.
- This triple is what a definition's canonical AST embeds at a primitive use site. The hash therefore includes the triple verbatim.
- Changing a primitive's behavior requires bumping its version. Every definition transitively depending on the primitive then gets a new hash.

**Honest cost.** Same discipline problem as procedure identity: a silent change to a primitive's native implementation without a version bump produces hashes that lie. Acceptable during bootstrapping; full state resets are an accepted fallback in this phase.

**Far-term alignment.** The metacircular goal from `02-definitions-and-derived-data.md` extends here: primitive behaviors could eventually be expressed in a meta-language and content-addressed, except for genuinely native primitives (FFI, machine arithmetic) where we'll likely still need a tag pointing to a native implementation. Deferred.

## Unison as prior art — what we adopt and what we don't

**Adopt (in spirit):**

- Per-definition content addressing with α-equivalent canonical forms.
- Hash-based references in stored programs; names in a separate layer.
- The property that hashes never dangle: a definition that exists always has all its dependencies present.
- Hashing mutually recursive groups as a single canonical unit (long-term; see open sub-questions).

**Do not adopt (current phase):**

- **Automatic migration UX.** When a definition changes, Unison helps users systematically move callers to the new hash. We have no equivalent. Callers remain pinned until an editing action explicitly rebinds them. *(Description corrected 2026-08-07 from a source read. This used to say "`update`, `patch`, etc.," but Unison's reified-edit machinery — `Patch`, `TermEdit`, `propagate` — was **deleted** in May 2025. What ships now is different: diff two namespaces at their lowest common ancestor, query a dependents index for what is affected, then **render those dependents back to source, re-parse, and re-typecheck** — its own code calls this "the world's weirdest implementation of AST substitution." Notably, that method is correct only if printing and re-parsing preserves hashes, which is `formalism/`'s `prop:print-elab`. See `../systems/unison/04-caching-update-merge.md` §"Update" and `../systems/unison/05-deletions.md` §"Patches and propagate".)*
- **Shared codebases and distribution.** Unison's networked code-sharing model is out of scope (non-goal in `00-overview.md`).
- **Their specific codebase representation** (base-32 textual hashes, wire format, `.u` file convention). Implementation choices we'll make independently when we get to the tech stack.

The "no silent breakage" property is what we get from the substrate alone: a call resolves to exactly the definition it referenced at authoring time, or the reference is dead in a visible way. Bulk-update and refactoring UX is additive, handled by the naming layer and interfaces.

**Sourcing.** This section cited nothing until 2026-08-07; it now rests on a source read of the implementation, pinned to a commit, in `../systems/unison/`. Note one adopted item where the two systems in fact diverge: Unison's hash-based references are **hash-transparent** — a reference node contributes the referent's hash directly, so inlining is identity-preserving and `y = x` *is* `x` — whereas `formalism/`'s `Ref(hash)` has its own encoding. Both keep the indirection; only the hashing differs. `../systems/unison/02-store-and-hashing.md` §"Reference transparency".

## Hash representation

Specifics (algorithm choice, encoding, length) are not locked in. Constraints:

- Deterministic across implementations.
- Collision-resistant enough that accidental collisions are impossible in practice.

Anything else is deferred. During bootstrap, if we need to change the representation, we accept full state rebuilds. We are not designing migration machinery for this phase.

**Prior art for the post-bootstrap version of this question** *(added 2026-08-07)*. Unison never rebuilds; it **versions the hash function inside its own input**. Every hash begins with a version token (`hashingVersion = Tag 2`), so bumping the version necessarily changes every hash and old and new can coexist in one table without colliding — and a `hash_object(hash_id, object_id, hash_version)` indirection lets several hashes name the same stored object, so references minted under an old scheme keep resolving. The type-level counterpart is that the *hashed* representation lives in its own frozen package that must never change, injected into the storage layer as a `HashHandle` value so the store has no compile-time dependency on it at all. None of this changes the stance above — full rebuilds remain the accepted bootstrap answer — but it is the shape the mature answer takes, and it is cheaper to know about before the format is load-bearing than after. `../systems/unison/02-store-and-hashing.md` §"Versioning", `../systems/unison/01-organization.md` §"Fact 2".

## Threads under exploration

### Hashing types as well as terms

So far only term-language definitions are content-addressed. Types appear inline in term encodings: an STLC `Lam(ty, body_hash)` writes `ty`'s structural bytes into the parent's hash input (p6, p9), and the `stlc:type-check:v1` aspect value `Type_of(Ty.t)` is an inline OCaml sum. p6 *does* hash types — but only to encode them compactly into procedure-identity suffixes (`lc-to-stlc:check:v1[ty=<hex8>]`), under a separate `'T'` tag so type hashes don't collide with definition hashes. The hash is a string-encoding tactic; types are not stored as definitions. Prototype seeds: `docs/prototypes/p6-stlc/open-questions.md` §"Hashing Ty" and §"Ty hash space"; `prototype-findings.md` §"Raised once" → "Hashed type space for non-Hash aspect values."

The thread is whether to promote types to first-class content-addressed objects. What changes concretely if so:

- **Term encodings carry type references.** `Lam(ty_hash, body_hash)` instead of inlining `ty`'s bytes. Type identity factors out of every term that mentions it; structural type equality becomes hash equality.
- **Types live in the store.** A type definition is a definition in some type-language; it has a hash; canonicalization for types (field order, type-variable α-equivalence once binders show up) is the type-language's job, paralleling term canonicalization.
- **Aspect values reference types by hash.** `Type_of(hash)` instead of `Type_of(Ty.t)`. The aspect-value sum stops carrying inline type ASTs; the type itself is reachable by hash.
- **Names can bind to type hashes.** This is the type-aliasing connection (below) and follows for free from `04-naming-layer.md`'s shape.

#### The type-aliasing reading

Pascal/Haskell-style type aliases are usually a separate language feature — `type` declarations, transparent expansion, scope rules of their own. If types are content-addressed *and* the namespace already binds names to hashes, an alias is just multiple names binding the same type hash, with the same semantics names already have for terms. `type Vector = Int -> Int -> Int` becomes a namespace bind; two names pointing at the same type hash are aliases by content, not by lookup-chasing.

Newtypes (distinct identity for an isomorphic carrier) remain a separate concept: they require the type-language to introduce a constructor that produces a *different* hash from its inner type. Alias vs. newtype thus becomes a type-language design choice, not a substrate one — the substrate doesn't need to know which it is. The substrate-level generalization of the newtype maneuver — opt-in opaque identity stamped into the canonical form for any definition, not just types — is sharpened in `10-minted-identity.md` as *minted identity*.

#### What's open

- **One global hash-space, or two?** "There is one global hash-space" (above) was stated for term-language definitions. Extending it to types is natural but unstated. If types share the space, every type definition needs a language tag — either a single meta-type-language, or per-target-language type-languages with disjoint tags. If types live in a separate space, the substrate gains a second store-shaped object and "one global hash-space" needs softening.
- **Cross-language type identity.** If STLC and a future STLC+subtyping both use `Int → Int`, do they share a type hash, or do their type-languages have disjoint tags? Sharing is appealing for translation (same type, same hash, same reverse-query results); not sharing keeps language identity rigid in the way `decisions.md` requires for terms. Probably bound up with the composition-model question in `01-language-model.md`.
- **When does it pay off?** For monomorphic primitives (`Int`, `Bool`, small `Arrow`s) inline embedding is small and the deduplication win is small. The case strengthens with polymorphic types (∀, ∃ — TAPL Ch. 23–24), recursive types (Ch. 20), and large structural records — types stop being trivially small, reverse queries ("which definitions have type T?") start mattering for type-based search, and the editing-layer story for naming a type gets worth telling.
- **F-omega lurking.** Once types are first-class objects with their own canonical form, the substrate is closer to F-omega's regime where types have their own type system (kinds — the word reserved for that). Adopting type-hashing too early may pre-shape decisions better deferred to when F-omega is being instantiated.
- **Migration cost.** Moving from inline `Type_of(Ty.t)` to `Type_of(hash)` is a one-time aspect-value-shape change, not a re-architecture; the discipline question is when the cost of inline embedding shows up in a prototype.

#### Connections

- `04-naming-layer.md` — names binding type hashes is the alias mechanism; no namespace schema change needed if types share the term hash-space.
- `02-definitions-and-derived-data.md` — `Type_of(...)` aspect-value shape is the touch point.
- `01-language-model.md` — content-addressed types nudge gently toward Option C (shared core IR) by giving types a canonical form independent of any specific term language.

### Mutual recursion: three ways to close the cycle

*(Opened 2026-08-07. Until then the open sub-question below recorded only Unison's approach and called it "the standard answer," which foreclosed a choice never actually made. **Revised the same day** — the first draft of this section claimed a labeled-record bundle made "the ordering problem disappear." That was wrong, and working out the positional-tuple variant is what showed it. See §"Correction" at the end. Nothing is chosen here.)*

The problem from §"Why content-address at all" restated: content addressing cannot name a thing that refers to itself. `even` calls `odd` and `odd` calls `even`, so neither hash can be computed first. **All three options convert the self-referential group into a closed object; they differ in *where* the closing happens and in *what the residual ambiguity costs*.**

#### What the ordering problem actually is

Worth naming before comparing, because it is not an artifact of any one encoding.

Every approach must eventually answer: **which member is which?** The members of a recursive group are distinguished only by their positions in a cycle of references. To give them stable identities you need a canonical order, and the order has to be a function of content alone — names cannot enter the store (`04-naming-layer.md`; `arbor-core.tex` `prop:namefree`).

Three properties are wanted, and they are not jointly free:

1. **Determinism** — the same input always yields the same hashes.
2. **Presentation-independence** — writing the group in a different order yields the *same* hashes. This is the α-equivalence argument applied to groups, and it is why source order is not an acceptable answer.
3. **Totality** — the procedure never fails.

The standard technique gets 1 and 2: hash each member in an environment where every intra-group reference is replaced by one indistinguishable marker, then sort by that hash. It gets 3 only when those hashes are distinct. **When two members are structurally identical modulo intra-group references, their hashes collide and the order is underdetermined.**

That failure is not fixable by a cleverer hash. Canonically ordering a recursive group is **graph canonization** — assigning canonical indices to the vertices of a labeled digraph — and the erase-and-sort trick is one round of the same colour-refinement idea that graph-isomorphism algorithms use, with the same known incompleteness. *(Stated as reasoning, not from a citation; `../related-work/01-content-addressing.md` has no entry for this and probably should.)*

So **every** option below faces the same ambiguity. What differs is where it lands and what happens when it bites.

#### Option A — the component, closed in the hasher

Unison's. Hash the whole strongly-connected group as one object and address a member by position: `Reference = (Hash, Pos)`. Intra-group references become indices rather than hashes. The recursion is broken inside the hashing algorithm, which carries a "cycle frame" environment in which every member resolves to the same de Bruijn index — that is the erase step, and sorting the resulting hashes is the canonical permutation. Details in `../systems/unison/02-store-and-hashing.md` §"Components".

**Cost 1: the store's type changes.** `Σ : Hash ⇀ Node` becomes `Σ : Hash ⇀ Node⁺`, and *every* reference in the system becomes a pair. That reaches `wf`'s no-dangle clause, `callers_of`, the migration rewrite, the namespace's codomain, and `thm:alpha` — see `formalism/open-questions.md`.

**Cost 2: on collision it refuses.** `Pos` is externally visible — it is in every reference and in every namespace binding — so Unison must commit to an index per member and will not guess. Hence `IncompleteElementOrderingError` and the instruction to perturb a definition by hand (unisonweb/unison#2787).

#### Option B1 — a fixpoint over a positional tuple

Close the cycle with a binder in the *object language* instead. Bundle the members into one ordinary definition, and make each member a separate ordinary definition that projects out of it.

```
bundle = fix (\self -> ( \n -> if n == 0 then true  else snd self (n-1)     -- even
                       , \n -> if n == 0 then false else fst self (n-1) ))  -- odd
even = fst bundle
odd  = snd bundle
```

The move is that **`self` is a bound variable, not a hash reference.** The bundle's body is closed, so it hashes like any other term; `even` and `odd` are unremarkable definitions containing a `Ref` and a projection.

**This is the cheapest option in machinery.** p17 already has `Node.Pair`/`Fst`/`Snd` and `Tnode.Product` (`prototypes/p17-translucent-modules/src/node.re:25-27`, `tnode.re:14`). Nothing is minted. The only addition is a `Fix` node with a CBV-safe reduction rule — p4 records that the Y combinator diverges under CBV (`docs/prototypes/p4-lambda-calculus/decisions.md`), so this cannot be encoded and needs a real node. `Product` is binary, so an *n*-member group nests; fix the nesting shape by convention (right-nested) and it contributes nothing to the hash.

**`Σ : Hash ⇀ Node` is unchanged.** A hash still names one node; a reference is still a hash. Nothing in `arbor-core.tex`'s configuration or metatheory changes shape.

**And the collision case degrades instead of failing.** This is the part that only became clear on working it through, and it is the strongest thing in favour of the bundle encodings.

Take the fully symmetric group — the one that defeats Option A:

```
f = \n -> if n == 0 then 0 else g (n-1)
g = \n -> if n == 0 then 0 else f (n-1)
```

Order the tuple `[f, g]` and you get `(λn.… snd self …, λn.… fst self …)`. Now order it `[g, f]`: slot 0 is `g`'s body, and `g` references `f`, which now sits at index 1, so slot 0 becomes `λn.… snd self …`. Slot 1 is `f`'s body referencing `g` at index 0, so `λn.… fst self …`. **The tuple is identical.** Renumbering the projections exactly undoes the permutation, so a group automorphism acts trivially on the bundle: *the bundle's hash is canonical regardless of which order you picked.*

The ambiguity does not vanish, though — it **moves to the projections**. Under `[f, g]` we get `f = fst bundle`; under `[g, f]`, `f = snd bundle`. Those are different definitions with different hashes. So presentation-independence is genuinely lost for the members in exactly the symmetric case.

But look at what a wrong guess *costs*. `fst bundle` and `snd bundle` here are structurally distinct and **observationally identical** — both are the same countdown function. So the consequence is a *coincidental non-collapse*: two hashes for one behaviour, the dual of the coincidental-convergence hazard `10-minted-identity.md` exists to handle. The store stays coherent, `wf` still holds, evaluation is unaffected, and nothing needs to refuse. Compare Option A, where the same input is rejected outright.

That is `feedback_visible_breakage`'s posture — commit and report, don't gate — applied to canonicalization. The substrate picks (source order is fine, since the bundle is invariant anyway), and the editing layer can surface "this group has a symmetry; the member/slot assignment was arbitrary" if anyone cares.

**Where B1 is genuinely weaker.** Presentation-independence for members is lost whenever the group has a nontrivial automorphism. Two people writing the same symmetric group in different orders get different hashes for `f`, which then do not deduplicate and do not share derived aspects. Rare, harmless when it happens, but real.

#### Option B2 — a fixpoint over a labeled record

Same construction, with p16/p17's records instead of tuples:

```
bundle = fix (\self -> { even = \n -> if n == 0 then true  else self#odd  (n-1)
                       , odd  = \n -> if n == 0 then false else self#even (n-1) })
even   = bundle#even
odd    = bundle#odd
```

`Tnode.Record` and `Node.Record_lit` canonicalize by **sorted label hash**, so field order carries no information (`tnode.re:98-102`), and members are distinguished by *label* rather than by position — including when their bodies are byte-identical.

**This restores presentation-independence in the symmetric case** — `bundle#even` is `bundle#even` regardless of how the source was written, because the projection names a label, not a slot.

**But it does not make canonicalization content-total, and the first draft of this section was wrong to say so.** It replaces a *content-derived* order with a *minted* one. Labels are bare mints (`11-label-sort.md`), so the identity that resolves the ambiguity is precisely the part of the hash that is not a function of content. Worse, the guarantee is only as good as **label stability**: `bundle#even` is stable across two authorings only if both resolve the name `even` to the *same* label. Per p16 that happens by "an already-bound field name reuses its label" — a **name-keyed mint lookup**, which is structurally the same mechanism as Unison's `loadUniqueTypeGuid` and inherits the same exposure (`../systems/unison/04-caching-update-merge.md` §"Minting": recovered by name, arbitrary tiebreak when the name is conflicted, extra machinery to survive merge).

So B2 does not eliminate the problem. It **converts a canonicalization question into a mint-stability question** — which may well be the better trade, since arbor has an explicit *edit-of-X* gesture and Unison does not, but it is a trade and not a dissolution.

#### Side by side

| | A — component | B1 — positional bundle | B2 — labeled bundle |
|---|---|---|---|
| Store type `Σ` | `Hash ⇀ Node⁺`; refs become pairs | **unchanged** | **unchanged** |
| Recursion closed in | the hashing algorithm | a binder in the object language | a binder in the object language |
| New machinery | SCC + cycle env in the hasher; pair-keyed refs everywhere | `Fix` node (+ p17's `Pair`/`Fst`/`Snd`) | `Fix` node (+ p17's records/labels) |
| Mints | none | **none** | one label per member |
| Member identity | position in a canonical order | position in the tuple | its label |
| Determinism | ✓ | ✓ | ✓ |
| Presentation-independence | ✓ where total | ✓ except under automorphism | ✓ *if labels are stable* |
| Totality | ✗ — refuses on collision | ✓ — arbitrary but harmless pick | ✓ — via a mint, not via content |
| Cost of the bad case | ingest rejected | two hashes for one behaviour | mint-stability exposure, name-keyed |
| Editing one member | rehashes the whole group | rehashes the whole bundle | rehashes the whole bundle |

Three things the table should not be read as hiding.

- **The dependency coarsening is identical in all three.** Editing any member changes every member's identity. None is finer-grained than the others, and none avoids the fact that a recursive group is one unit of change.
- **A leans on machinery arbor does not have; B1 and B2 lean on machinery arbor does.** `Pair`/`Fst`/`Snd`, records, labels, and projection are all built and exercised in p16/p17; nothing in any prototype has a component-shaped store.
- **B1 is the only option that needs no mint**, which makes it the one that keeps a recursive group a purely structural object. If the mint-vs-canonicalization trade is the crux — and it looks like it is — B1 and B2 are the two ends of it, and A is the version that refuses to make the trade at all.

#### Correction

The first draft of this section (2026-08-07, earlier the same day) said a labeled-record bundle made "the ordering problem disappear" and "dissolved the partiality problem outright," and dismissed the positional variant as simply reinheriting Option A's problem. Both halves were wrong, and in opposite directions:

- **B2 does not dissolve the problem**; it relocates it from content-canonicalization to name-keyed mint stability.
- **B1 does not reinherit Option A's problem** in the form that matters. The bundle's hash is invariant under group automorphisms, so the collision case leaves only an arbitrary-but-behaviour-preserving choice of which projection is which — a coincidental non-collapse, not a hard failure.

The net effect is that the positional variant is *more* attractive than the first draft suggested, not less, and the real fork is **mint versus a rare loss of presentation-independence** rather than "totality versus mints."

**One argument the comparison above is missing: A's advantage is notational only.** Both closures make the strongly-connected component the unit of identity. Under A, editing one member changes the component hash, so every sibling's address changes too even though its source did not. Under B, editing one member re-mints the bundle, so every sibling's projection changes. The granularity is *the same*, and so is the coarseness people find surprising — this is not a place where A is more precise. What A buys is that each member still looks like a top-level definition, addressed in its own right; what it costs is that the identity function becomes **partial** (canonical ordering of a cycle is graph canonization, and the mature implementation's answer to the hard case is to ask the user to add a dummy binding) and that the *type of the store* changes, which propagates to every reference, every aspect key, and `N : Name ⇀ Hash`. Trading totality of the one function the whole design rests on, plus a change across all four layers, for a notational convenience looks like the wrong side of the trade. There is also a layering argument specific to this substrate: `Definition.t` is a sum over languages, so putting cycle-closing in the *hasher* makes every language inherit the ordering partiality whether it has recursion or not, while putting it in the object language lets each language close cycles its own way — or not have recursion at all. None of this settles B1 versus B2, which is still the mint-versus-presentation-independence fork above.

**Not chosen.** No prototype has needed recursion (`prototype-findings.md` §"Mutual recursion canonicalization"), so this stays a thread. The point of writing it down now is that the open sub-question below previously named exactly one option and called it standard, which foreclosed a choice arbor has not actually made. Evaluating B properly wants a prototype with `Fix` plus the existing record machinery; recorded in `../systems/unison/10-followups.md`.

## Non-goals (current phase)

- **Migration machinery.** Automatically moving callers when a definition changes.
- **Distribution, shared codebases, networked publishing.**
- **Locking in a hash algorithm.**
- **Hashing partial / ill-formed programs.** The long-term direction here is different from how it first sounds: hole-aware languages (from the Hazel lineage) will encode incompleteness as first-class AST constructs, so a "program with holes" in a hole-aware language is not partial — it's a well-formed program in a richer language, hashable and canonicalizable like any other. We don't need generic "partial program" machinery in the substrate; we just need each hole-aware language to declare its own canonical form when it's introduced. Deferred until we add one.

## Open sub-questions

Tracked in `open-questions.md` under "Content addressing."

- **Mutual recursion canonicalization.** When a language introduces mutually recursive definitions, the substrate needs a way to close the cycle. Not urgent until a language introduces recursion. **Three options, worked through above in §"Mutual recursion: three ways to close the cycle" (2026-08-07); none chosen.** *(A) Unison's component: hash the group as one object, address members by index. Costs a change to the store's type — `Σ : Hash ⇀ Node⁺`, every reference a pair — and **refuses** when two members are structurally identical modulo intra-group references (`IncompleteElementOrderingError`, unisonweb/unison#2787). (B1) A fixpoint over a **positional tuple**, members as projections: `Σ` unchanged, no mints, reuses p17's `Pair`/`Fst`/`Snd`; the bundle's hash is invariant under group automorphisms, so the bad case costs only an arbitrary-but-behaviour-preserving choice of which projection is which. (B2) The same over a **labeled record**: restores presentation-independence in that case, but by converting canonicalization into name-keyed mint stability, not by dissolving it. The real fork is **a mint versus a rare loss of presentation-independence**; ordering a recursive group is graph canonization, so no cleverer hash rescues (A). This bullet previously named only (A) and called it "a known starting point," which foreclosed a choice not actually made. See `../systems/unison/02-store-and-hashing.md` §"Components".)*
- **Holes and incomplete programs.** If the substrate eventually hashes incomplete programs (Hazel-style editing workflows), how do holes participate in the canonical form? Unique hole identities, wildcards that make hash matching a subsumption relation, or something else?
- **Cross-version primitive aliasing.** If `int:add:v1` and `int:add:v2` differ only cosmetically, callers of v1 are orphaned. Is there an aliasing story, or is this just accepted cost of the manual-version discipline?
- **Hashing types as well as terms.** Whether to promote types to first-class content-addressed objects — making type aliasing a free consequence of the namespace and changing how type-valued aspects are shaped. Sharpened above under *Threads under exploration*.
- **Minted identity.** Whether definitions can opt into an opaque, non-stringly-typed identity stamped into their canonical form, so coincidentally-equal structural twins do not deduplicate and the substrate has a stable handle for editing history and update propagation. Sharpened in `10-minted-identity.md`.
