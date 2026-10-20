# Content Addressing

**Status:** Draft.

## Purpose

How definitions are identified, how they reference each other, and how their hashes are grounded at the leaves. This doc covers only the hashing substrate. Caching of derived associated data is in `02-definitions-and-derived-data.md`; translation-specific caching is in `05-translation.md`.

## Why content addressing (added 2026-08-03)

Until 2026-08-03 this doc described how we hash without arguing why. The question matters because minting (`10-minted-identity.md`) makes a live alternative of issuing a fresh key per definition and keeping a `key → data` map. That alternative is simpler, never collides, and gives lineage for free.

Content addressing buys four things a minted-key map cannot. All four follow from one property: anyone can derive the identity from the thing itself, without asking permission.

- **Verification without trust.** A recipient can recompute and check the hash, so the store need not be trusted and any mirror, peer, or agent can serve a definition. A `key → data` map requires trusting whoever holds the map.
- **Agreement without coordination.** Two parties who independently write the same definition arrive at the same name without communicating. Under minting they get different names and can never discover the match. This is easy to misattribute to immutability, but an append-only minted store is fully immutable and does not converge at all.
- **Cache keys that are theorems.** Same hash implies same content implies same derived result. This is what makes the eval-stability property in `02-definitions-and-derived-data.md` provable. Under minting, "these two keys denote equal content" is a separate fact someone must establish and maintain.
- **Immutability by arithmetic.** Content cannot change without the name changing. An append-only discipline over minted keys has the same effect, but a discipline can be violated and hashing cannot.

Deduplication, the most often cited benefit, matters least for us: decisive at archive scale, near-irrelevant at ours.

The converse is why `10-minted-identity.md` exists. Every property above concerns sameness; none concerns identity through time. Stable identity across edits, deliberate distinctness of coincidentally equal definitions, and "this moved" are things content addressing cannot express and minting provides. A serious store needs both layers, as Git, Nix, IPFS, Perkeep, and Software Heritage all have. Our unusual choice is placement: the mint sits inside the hashed bytes, in the same node as semantic content. Perkeep instead uses separate content-free identity blobs, and Git keeps refs outside the object store. Tracked in `open-questions.md` §"Minted identity".

The hard limit is self-reference. Content addressing cannot name a thing that refers to itself, since the name depends on content that depends on the name. Nix meets the same wall with self-referential store paths and concedes that its hash-rewriting workaround is only a heuristic. Our contents are structured syntax, not opaque bytes, which admits more than one way out; §"Mutual recursion: three ways to close the cycle" works through three.

Sources and the fuller argument: `../related-work/01-content-addressing.md` §"Why content-address at all, rather than mint everything?".

## Identity

A definition is identified by its hash alone. Cryptographic hash collisions are treated as impossible in practice.

- **Hash → definition** is a globally unique lookup. A hash returns the full definition, which carries its language as a property.
- **Language is associated data on the definition,** not a component of identity. This is a modeling convenience and does not weaken the "no cross-language references" rule, which is enforced at construction time (below).
- There is one global hash space, not partitioned by language.

## Hash input: canonicalized AST

The hash is computed over a canonicalized form of the definition's AST. Each language declares its canonicalizer. At minimum every language collapses **α-equivalence**, so `λx.x` and `λy.y` hash identically.

Canonicalization is **syntactic by default.** Coarser equivalences (β- or η-normalization, normal form up to reduction, extensional equality) are a per-language opt-in nobody has exercised yet. A language may declare a more aggressive canonicalizer if that is the experiment it wants to run. Coarser equivalences discard distinctions that matter for resource behavior and programmer intent, so the choice has a cost.

Other canonicalizations a language might declare:

- Normalizing record or variant field order, if the semantics treat fields as unordered. `11-label-sort.md` gives the substrate's preferred treatment: once labels are content-addressed hashes, sorting by label hash is the natural canonical order.
- Canonicalizing the presentation order of mutually recursive members, once recursion is added.
- Normalizing type-variable names in languages with explicit type binders.

## References in stored programs

Every cross-definition reference in a stored program is a hash reference, `Ref(hash)`, never a name.

- **Names never appear in stored programs.** Name resolution happens at edit time and produces a hash reference.
- **No cross-language references.** Per `decisions.md`, a definition in L1 cannot reference a definition in L2. The substrate enforces this at registration: every `Ref(hash)` in the body must resolve to a definition in the referring language, and references that span languages are rejected. Reuse across languages goes through translation.
- **Callers pin automatically.** Editing a definition produces a new hash, and existing callers still reference the old one. Nothing migrates silently. Upgrading callers is the editing layer's job; see `04-naming-layer.md`.
- **Hashes ground transitively.** A definition's hash depends on the hashes of what it references, and so on down to primitives.

## Primitive identity

Primitives are language builtins with no body to hash over, and they ground the hash chain. They follow the procedure-identity model from `02-definitions-and-derived-data.md`: **manually versioned tags.**

- A primitive is identified by `(namespaced-tag, version, signature)`, e.g., `int:add:v1` with a declared signature in its language.
- A definition's canonical AST embeds this triple at each primitive use site, so the hash includes it verbatim.
- Changing a primitive's behavior requires bumping its version, which gives every transitively dependent definition a new hash.

**Cost.** The same discipline problem as procedure identity: changing a primitive's native implementation without a version bump produces hashes that lie. Acceptable during bootstrap, with full state resets as the fallback.

**Far-term.** The metacircular goal from `02-definitions-and-derived-data.md` extends here. Primitive behaviors could eventually be written in a meta-language and content-addressed, except for genuinely native primitives (FFI, machine arithmetic), which will likely still need a tag pointing at a native implementation. Deferred.

## Unison as prior art — what we adopt and what we don't

**Adopted in spirit:**

- Per-definition content addressing with α-equivalent canonical forms.
- Hash-based references in stored programs, with names in a separate layer.
- Hashes never dangle: a definition that exists always has all its dependencies present.
- Hashing mutually recursive groups as one canonical unit (long-term; one of three options in §"Mutual recursion: three ways to close the cycle").

**Not adopted (current phase):**

- **Automatic migration UX.** When a definition changes, Unison helps users move callers to the new hash. We have no equivalent; callers stay pinned until an editing action rebinds them. This bullet used to cite "`update`, `patch`, etc." and was corrected on 2026-08-07 from a source read: Unison deleted its reified-edit machinery (`Patch`, `TermEdit`, `propagate`) in May 2025. What ships now diffs two namespaces at their lowest common ancestor, queries a dependents index for what is affected, and then renders those dependents back to source, re-parses, and re-typechecks them. Unison's own code calls this "the world's weirdest implementation of AST substitution." The method is correct only if printing and re-parsing preserves hashes, which is `formalism/`'s `prop:print-elab`. See `../systems/unison/04-caching-update-merge.md` §"Update" and `../systems/unison/05-deletions.md` §"Patches and propagate".
- **Shared codebases and distribution.** Out of scope (non-goal in `00-overview.md`).
- **Unison's codebase representation** (base-32 textual hashes, wire format, `.u` files). We will make these choices independently.

The substrate alone gives "no silent breakage": a call resolves to exactly the definition it referenced at authoring time, or the reference is visibly dead. Bulk update and refactoring UX are additive, provided by the naming layer and interfaces.

**Sourcing.** This section cited nothing until 2026-08-07. It now rests on a source read pinned to a commit, in `../systems/unison/`. One real difference: Unison's references are hash-transparent at position 0, so a reference node contributes the referent's hash verbatim, whereas `formalism/`'s `Ref(hash)` has its own tagged encoding. Both keep the indirection; only the hashing differs.

Until 2026-09-02 that paragraph also claimed that transparency makes inlining identity-preserving, so that `y = x` *is* `x`. This is false. Transparency passes along the referent's component hash, inlining would contribute its term hash, and component hashing separates the two even for a singleton definition. Inlining changes identity in Unison as it does here. See `../systems/unison/02-store-and-hashing.md` §"Reference transparency"; computed in `prototypes/p21-unison-model/`.

## Hash representation

Algorithm, encoding, and length are not locked in. Constraints:

- Deterministic across implementations.
- Collision-resistant enough that accidental collisions are impossible in practice.

Everything else is deferred. During bootstrap, changing the representation means a full state rebuild; no migration machinery is designed for this phase.

**Prior art for the post-bootstrap version** (added 2026-08-07). Unison never rebuilds. It versions the hash function inside its own input: every hash begins with a version token (`hashingVersion = Tag 2`), so bumping the version changes every hash and old and new hashes coexist in one table without colliding. A `hash_object(hash_id, object_id, hash_version)` indirection lets several hashes name the same stored object, so references minted under an old scheme keep resolving. At the type level, the hashed representation lives in its own frozen package that must never change, injected into the storage layer as a `HashHandle` value so the store has no compile-time dependency on it. This does not change the stance above; full rebuilds remain the bootstrap answer. It is the shape the mature answer takes, and cheaper to know before the format is load-bearing. `../systems/unison/02-store-and-hashing.md` §"Versioning", `../systems/unison/01-organization.md` §"Fact 2".

## Threads under exploration

### Hashing types as well as terms

So far only term-language definitions are content-addressed. Types appear inline in term encodings: an STLC `Lam(ty, body_hash)` writes `ty`'s structural bytes into the parent's hash input (p6, p9), and the `stlc:type-check:v1` aspect value `Type_of(Ty.t)` is an inline OCaml sum. p6 does hash types, but only to encode them compactly into procedure-identity suffixes (`lc-to-stlc:check:v1[ty=<hex8>]`), under a separate `'T'` tag so type hashes cannot collide with definition hashes. That is a string-encoding tactic; types are not stored as definitions. Prototype seeds: `docs/prototypes/p6-stlc/open-questions.md` §"Hashing Ty" and §"Ty hash space"; `prototype-findings.md` §"Raised once" → "Hashed type space for non-Hash aspect values."

The thread is whether to make types first-class content-addressed objects. Concretely, that would mean:

- **Term encodings carry type references.** `Lam(ty_hash, body_hash)` instead of inline bytes. Type identity factors out of every term that mentions it, and structural type equality becomes hash equality.
- **Types live in the store.** A type is a definition in some type language, with a hash. Canonicalizing types (field order, α-equivalence of type variables once binders appear) is the type language's job, parallel to term canonicalization.
- **Aspect values reference types by hash.** `Type_of(hash)` instead of `Type_of(Ty.t)`; the aspect-value sum stops carrying inline type ASTs.
- **Names can bind to type hashes.** This gives type aliasing for free from the shape of `04-naming-layer.md` (below).

#### The type-aliasing reading

Pascal- or Haskell-style type aliases are usually a separate language feature, with `type` declarations, transparent expansion, and their own scope rules. If types are content-addressed and the namespace already binds names to hashes, an alias is just several names bound to one type hash, with the semantics names already have for terms. `type Vector = Int -> Int -> Int` becomes a namespace bind, and two names for the same type hash are aliases by content, with no lookup chain.

Newtypes, which give an isomorphic carrier distinct identity, remain separate: the type language must introduce a constructor whose hash differs from its inner type's. Alias vs. newtype thus becomes a type-language choice the substrate need not know about. `10-minted-identity.md` generalizes the newtype move to the substrate as *minted identity*: opt-in opaque identity stamped into the canonical form of any definition, not just types.

#### What's open

- **One global hash space, or two?** "One global hash space" (above) was stated for term-language definitions; extending it to types is natural but unstated. If types share the space, every type definition needs a language tag, either one meta type language or per-language type languages with disjoint tags. If types get their own space, the substrate gains a second store-shaped object and the claim needs softening.
- **Cross-language type identity.** If STLC and a future STLC+subtyping both use `Int → Int`, do they share a type hash? Sharing helps translation (same type, same hash, same reverse-query results). Not sharing keeps language identity as rigid as `decisions.md` requires for terms. Probably tied to the composition-model question in `01-language-model.md`.
- **When does it pay off?** For monomorphic primitives (`Int`, `Bool`, small `Arrow`s), inline embedding and the deduplication win are both small. The case strengthens with polymorphic types (∀, ∃; TAPL Ch. 23–24), recursive types (Ch. 20), and large structural records. Types then stop being trivially small, reverse queries ("which definitions have type T?") start to matter for type-based search, and naming types in the editing layer becomes worth doing.
- **F-omega.** Once types are first-class objects with their own canonical form, the substrate is closer to F-omega, where types have their own type system (kinds, the word reserved for that). Adopting type hashing early may pre-shape decisions better made when F-omega is instantiated.
- **Migration cost.** Moving from `Type_of(Ty.t)` to `Type_of(hash)` is a one-time change of aspect-value shape, not a re-architecture. The question is when the cost of inline embedding shows up in a prototype.

#### Connections

- `04-naming-layer.md`: names bound to type hashes are the alias mechanism. No namespace schema change is needed if types share the term hash space.
- `02-definitions-and-derived-data.md`: the `Type_of(...)` aspect-value shape is the touch point.
- `01-language-model.md`: content-addressed types nudge toward Option C (shared core IR) by giving types a canonical form independent of any term language.

### Mutual recursion: three ways to close the cycle

Opened 2026-08-07; nothing is chosen. Before that date the open sub-question below recorded only Unison's approach and called it "the standard answer", foreclosing a choice never made. The first draft of this section, from the same day, was also wrong in a way §"Correction" records.

`even` calls `odd` and `odd` calls `even`, so neither hash can be computed first. Each option below turns the self-referential group into a closed object. They differ in where the closing happens and what the residual ambiguity costs.

#### The ordering problem

Every approach must decide which member is which. Members of a recursive group are distinguished only by their positions in a cycle of references. Stable identities need a canonical order, and that order must be a function of content alone, because names cannot enter the store (`04-naming-layer.md`; `arbor-core.tex` `prop:namefree`).

Three properties are wanted, and they are not jointly free:

1. **Determinism.** The same input always yields the same hashes.
2. **Presentation-independence.** Writing the group in a different order yields the same hashes. This is the α-equivalence argument applied to groups, and it rules out source order.
3. **Totality.** The procedure never fails.

The standard technique gets 1 and 2. It hashes each member in an environment where every intra-group reference is replaced by one indistinguishable marker, then sorts by that hash. It gets 3 only when those hashes are distinct. When two members are structurally identical modulo intra-group references, their hashes collide and the order is underdetermined.

A cleverer hash cannot fix this. Canonically ordering a recursive group is graph canonization (assigning canonical indices to the vertices of a labeled digraph), and erase-and-sort is one round of the colour refinement that graph-isomorphism algorithms use, with the same known incompleteness. This is stated as reasoning, not from a citation; `../related-work/01-content-addressing.md` has no entry for it and probably should. Every option below faces the same ambiguity. They differ in where it lands and what happens when it bites.

#### Option A — the component, closed in the hasher

Unison's approach. Hash the whole strongly connected group as one object and address a member by position: `Reference = (Hash, Pos)`. Intra-group references become indices. The recursion is broken inside the hashing algorithm, which carries a "cycle frame" environment in which every member resolves to the same de Bruijn index. That is the erase step, and sorting the resulting hashes gives the canonical permutation. Details in `../systems/unison/02-store-and-hashing.md` §"Components".

**Cost 1: the store's type changes.** `Σ : Hash ⇀ Node` becomes `Σ : Hash ⇀ Node⁺`, and every reference in the system becomes a pair. That reaches `wf`'s no-dangle clause, `callers_of`, the migration rewrite, the namespace's codomain, and `thm:alpha`; see `formalism/open-questions.md`.

**Cost 2: on collision it aborts.** `Pos` is externally visible, in every reference and every namespace binding, so Unison must commit to an index per member. Hence `IncompleteElementOrderingError` and the instruction to perturb a definition by hand (unisonweb/unison#2787).

A precision added 2026-08-25 from `formalism/paper/mini-unison.tex`: the hasher does guess. It falls back to a stable sort over the name-ordered input, which makes the component's hash a function of the members' names. It detects that it has done so, and the caller aborts instead of storing that hash. So the property that fails is presentation-independence, not definedness, and the check is a decidable side condition, which is the shape any arbor treatment would inherit.

#### Option B1 — a fixpoint over a positional tuple

Close the cycle with a binder in the object language. Bundle the members into one ordinary definition, and make each member a separate ordinary definition that projects out of it.

```
bundle = fix (\self -> ( \n -> if n == 0 then true  else snd self (n-1)     -- even
                       , \n -> if n == 0 then false else fst self (n-1) ))  -- odd
even = fst bundle
odd  = snd bundle
```

`self` is a bound variable, not a hash reference. The bundle's body is closed, so it hashes like any other term, and `even` and `odd` are plain definitions containing a `Ref` and a projection.

**Machinery.** This is the cheapest option. p17 already has `Node.Pair`/`Fst`/`Snd` and `Tnode.Product` (`prototypes/p17-translucent-modules/src/node.re:25-27`, `tnode.re:14`), and nothing is minted. The only addition is a `Fix` node with a CBV-safe reduction rule. It cannot be encoded, because p4 records that the Y combinator diverges under CBV (`docs/prototypes/p4-lambda-calculus/decisions.md`). `Product` is binary, so an *n*-member group nests; fixing the nesting shape by convention (right-nested) keeps it out of the hash.

**`Σ : Hash ⇀ Node` is unchanged.** A hash still names one node and a reference is still a hash. Nothing in `arbor-core.tex`'s configuration or metatheory changes shape.

**The collision case degrades instead of failing.** This only became clear when the case was worked through, and it is the strongest argument for the bundle encodings. Take the fully symmetric group that defeats Option A:

```
f = \n -> if n == 0 then 0 else g (n-1)
g = \n -> if n == 0 then 0 else f (n-1)
```

Order the tuple `[f, g]` and you get `(λn.… snd self …, λn.… fst self …)`. Order it `[g, f]`: slot 0 is `g`'s body, and `g` references `f`, now at index 1, so slot 0 is `λn.… snd self …`. Slot 1 is `f`'s body referencing `g` at index 0, so `λn.… fst self …`. The tuple is identical. Renumbering the projections undoes the permutation, so a group automorphism acts trivially on the bundle, and the bundle's hash is canonical whichever order was picked.

The ambiguity moves to the projections. Under `[f, g]`, `f = fst bundle`; under `[g, f]`, `f = snd bundle`. Those are different definitions with different hashes, so presentation-independence is lost for the members in the symmetric case. Two people writing the same symmetric group in different orders get different hashes for `f`, which then neither deduplicate nor share derived aspects.

The cost of a wrong guess is small. `fst bundle` and `snd bundle` are structurally distinct and observationally identical, both the same countdown function. The result is a coincidental non-collapse, two hashes for one behaviour, which is the dual of the coincidental-convergence hazard `10-minted-identity.md` handles. The store stays coherent, `wf` holds, evaluation is unaffected, and nothing needs to refuse; Option A rejects the same input outright. This is the commit-and-report posture of `feedback_visible_breakage` applied to canonicalization. The substrate picks (source order suffices, since the bundle is invariant), and the editing layer can report that the group has a symmetry and the member/slot assignment was arbitrary.

#### Option B2 — a fixpoint over a labeled record

The same construction with p16/p17's records instead of tuples:

```
bundle = fix (\self -> { even = \n -> if n == 0 then true  else self#odd  (n-1)
                       , odd  = \n -> if n == 0 then false else self#even (n-1) })
even   = bundle#even
odd    = bundle#odd
```

`Tnode.Record` and `Node.Record_lit` canonicalize by sorted label hash, so field order carries no information (`tnode.re:98-102`). Members are distinguished by label, not position, even when their bodies are byte-identical. This restores presentation-independence in the symmetric case: `bundle#even` is `bundle#even` however the source was written, because the projection names a label.

It does not make canonicalization content-total. It replaces a content-derived order with a minted one. Labels are bare mints (`11-label-sort.md`), so what resolves the ambiguity is the part of the hash that is not a function of content. The guarantee is also only as good as label stability. `bundle#even` is stable across two authorings only if both resolve the name `even` to the same label, which per p16 happens because "an already-bound field name reuses its label". That is a name-keyed mint lookup, structurally the same mechanism as Unison's `loadUniqueTypeGuid`, with the same exposure: recovered by name, an arbitrary tiebreak when the name is conflicted, and extra machinery to survive merge (`../systems/unison/04-caching-update-merge.md` §"Minting").

B2 therefore converts a canonicalization question into a mint-stability question. That may be the better trade, since arbor has an explicit *edit-of-X* gesture and Unison does not, but the problem is relocated, not eliminated.

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

Notes on reading the table:

- **Granularity is identical in all three.** All make the strongly connected component the unit of identity. Under A, editing one member changes the component hash, so every sibling's address changes though its source did not. Under B, editing one member re-mints the bundle, so every sibling's projection changes. A is not more precise here.
- **A's advantage is notational.** Each member still looks like a top-level definition, addressed in its own right. The price is that the identity function becomes partial (the mature implementation's answer to the hard case is to ask the user to add a dummy binding) and that the store's type changes, which propagates to every reference, every aspect key, and `N : Name ⇀ Hash`. Giving up totality of the function the whole design rests on, plus a change across all four layers, for a notational convenience looks like the wrong side of the trade.
- **Layering.** `Definition.t` is a sum over languages. Closing cycles in the hasher makes every language inherit the ordering partiality whether or not it has recursion. Closing them in the object language lets each language close cycles its own way, or have no recursion at all.
- **A needs machinery arbor lacks; B1 and B2 use machinery arbor has.** `Pair`/`Fst`/`Snd`, records, labels, and projection are built and exercised in p16/p17. No prototype has a component-shaped store.
- **B1 is the only option needing no mint**, so it keeps a recursive group purely structural. If the mint-vs-canonicalization trade is the crux, as it appears to be, B1 and B2 are its two ends, and A declines to make the trade. None of the above settles B1 versus B2.

#### Correction

The first draft of this section (2026-08-07, earlier the same day) said a labeled-record bundle made "the ordering problem disappear" and "dissolved the partiality problem outright", and dismissed the positional variant as reinheriting Option A's problem. Both halves were wrong, in opposite directions. B2 relocates the problem from content canonicalization to name-keyed mint stability. B1 does not reinherit A's problem in the form that matters: its bundle hash is invariant under group automorphisms, so a collision leaves only a behaviour-preserving choice of which projection is which. The positional variant is more attractive than the first draft suggested, and the real fork is a mint versus a rare loss of presentation-independence, not totality versus mints.

#### Status

Not chosen. No prototype has needed recursion (`prototype-findings.md` §"Mutual recursion canonicalization"), so this stays a thread. Evaluating B properly wants a prototype with `Fix` plus the existing record machinery, recorded in `../systems/unison/10-followups.md`.

## Non-goals (current phase)

- **Migration machinery** that moves callers automatically when a definition changes.
- **Distribution, shared codebases, networked publishing.**
- **Locking in a hash algorithm.**
- **Hashing partial or ill-formed programs.** Hole-aware languages (from the Hazel lineage) encode incompleteness as first-class AST constructs, so a program with holes in such a language is a well-formed program in a richer language, hashable and canonicalizable like any other. The substrate needs no generic partial-program machinery, only that each hole-aware language declare its canonical form when introduced. Deferred until we add one.

## Open sub-questions

Tracked in `open-questions.md` under "Content addressing."

- **Mutual recursion canonicalization.** A language that introduces mutual recursion needs a way to close the cycle. Three options are worked through in §"Mutual recursion: three ways to close the cycle" (2026-08-07), none chosen. (A) Unison's component changes the store's type and refuses when two members are structurally identical modulo intra-group references (`IncompleteElementOrderingError`, unisonweb/unison#2787). (B1) A fixpoint over a positional tuple leaves `Σ` unchanged, mints nothing, and reuses p17's `Pair`/`Fst`/`Snd`; its bad case is an arbitrary but behaviour-preserving choice of projection. (B2) A labeled record restores presentation-independence by converting canonicalization into name-keyed mint stability. The fork is a mint versus a rare loss of presentation-independence. See also `../systems/unison/02-store-and-hashing.md` §"Components".
- **Holes and incomplete programs.** If the substrate eventually hashes incomplete programs (Hazel-style editing), how do holes participate in the canonical form? Unique hole identities, wildcards that make hash matching a subsumption relation, or something else?
- **Cross-version primitive aliasing.** If `int:add:v1` and `int:add:v2` differ only cosmetically, callers of v1 are orphaned. Is there an aliasing story, or is this an accepted cost of manual versioning?
- **Hashing types as well as terms.** Whether types become first-class content-addressed objects, making type aliasing a consequence of the namespace and changing the shape of type-valued aspects. See *Threads under exploration* above.
- **Minted identity.** Whether definitions can opt into an opaque, non-string identity stamped into their canonical form, so coincidentally equal structural twins do not deduplicate and the substrate has a stable handle for editing history and update propagation. See `10-minted-identity.md`.
