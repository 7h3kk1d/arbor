# Unison — The namespace and its history

**Status:** Source read @ `db60ce2`. See `00-overview.md` for the pin and scope. Everything is `[source-verified @ db60ce2]` unless tagged otherwise.

## The namespace is a hashed object, not a side table

This is the first thing that does not match arbor's architecture. In `../../design/06-architecture.md` the namespace lives in the **Attachment** layer, above the Store. In Unison the namespace *is* a stored object: it has a hash, it lives in the `object` table alongside terms and decls (`object_type_description` id 2, `sql/create.sql:55-64`), and it is content-addressed by the same frozen hashing package.

```haskell
data Branch = Branch
  { terms    :: Map NameSegment (Map Referent MdValues)
  , types    :: Map NameSegment (Map Reference MdValues)
  , patches  :: Map NameSegment Hash
  , children :: Map NameSegment Hash   -- the child's CAUSAL hash
  }
instance Tokenizable Branch where
  tokens b = [accumulateToken (terms b), accumulateToken (types b),
              accumulateToken (children b), accumulateToken (patches b)]
```
`unison-hashing-v2/src/Unison/Hashing/V2/Branch.hs:22-39`.

**The load-bearing detail is that `children` maps to the child's *causal* hash, not its branch hash.** Every level of the namespace tree therefore has its own independent history, and a parent commit pins a specific point in each child's history. `parser-typechecker/src/Unison/Codebase/Branch/Type.hs:102-103` states it: *"Note the 'Branch' here, not 'Branch0'. Every level in the tree has a history."*

There are three representations, in the pattern from `01-organization.md` §"Fact 1":

| | Where | Shape |
|---|---|---|
| V1, eager | `parser-typechecker/src/Unison/Codebase/Branch/Type.hs:69-120` | `Branch = Causal Branch0`; `Branch0` uses `Star` relations and carries five derived cache fields |
| V2, lazy | `codebase2/codebase/U/Codebase/Branch/Type.hs:38-43` | every field wrapped in `m`, children load on demand |
| On disk | `codebase2/codebase-sqlite/U/Codebase/Sqlite/Branch/Full.hs`, `Branch/Diff.hs` | whole-branch or diff-against-parent |

There is also a **V3** shape, forward-looking and trimmed (`codebase2/codebase/U/Codebase/BranchV3.hs:16-30`): *"A V3 branch is a trimmed-down V2 branch: Names can't be conflicted. Metadata doesn't exist. Patches don't exist."* All three of those removals are the subject of `05-deletions.md`. It still hashes *through* the V2 branch type with empty patches and metadata (`Convert2.hs:45-54`), preserving hash-format compatibility — a clean illustration of the frozen-format discipline from `01-organization.md` §"Fact 2". And an honest comment at `BranchV3.hs:28-29`: *"A V3 branch's history has V3 branches everywhere at the latest causal … but when we go back in history, we find V2 branches, because that's what we used to have ;)"*

**Observation for arbor.** Putting the namespace in the store rather than beside it is what makes namespace *history* content-addressed for free, and what makes `merge` an operation on hashes rather than on mutable state. arbor's `formalism/paper/arbor-core.tex` models `N` as a partial function held alongside `Σ`, and `H` as a separate per-name append-only log. That is the right shape for a single namespace; it is not the shape that generalizes to branching. See `07-arbor-mapping.md` §"History".

## Names map to *sets*, and conflicts are first-class

`Map NameSegment (Map Referent _)` is a relation, not a function. One name may denote several referents, and one referent may have several names. Both directions are ordinary.

The V1 branch carries a derived field whose type says the whole story:

```haskell
asUnconflicted :: Either (Defn (Conflicted Name Referent) (Conflicted Name TypeReference))
                         UnconflictedLocalDefnsView
```
`parser-typechecker/src/Unison/Codebase/Branch/Type.hs:113-119`, recomputed by `deriveAsUnconflicted` (`:291-314`), which fails on any name with more than one referent.

Commands that cannot tolerate ambiguity assert unconflictedness up front rather than resolving it — e.g. `handleUpdate2` (`unison-cli/.../Update2.hs:99-102`) checks first and emits `Output.ConflictedDefn`. The database agrees: the name-lookup primary key includes the referent columns, so several rows per name are legal (`sql/create.sql:271`).

**Observation for arbor.** `formalism/decisions.md` records "naming at the substrate primitive (exact resolution)" with an explicit *"Honest divergence from the prototypes"* paragraph. Unison is the mature counterexample: after years of use, the namespace is a relation, conflict is a representable state rather than an error at bind time, and *unconflictedness is a derived view that operations demand when they need it*. That is precisely the `feedback_visible_breakage` posture — commit and report, don't gate — applied to names rather than types. arbor's `N : Name ⇀ Hash` is the simplification, and it is worth saying so in the paper rather than leaving it as the obvious choice.

## `Referent` vs `Reference` — names can denote non-definitions

```haskell
data ConstructorType = DataConstructor | EffectConstructor
data Referent' termRef typeRef = Ref termRef | Con typeRef ConstructorId
```
`codebase2/codebase/U/Codebase/Referent.hs:18-30`. The V1 doc comment is the clearest statement (`unison-core/src/Unison/Referent.hs:52-66`): *"Slightly odd naming. This is the 'referent of term name in the codebase', rather than the target of a `Reference`."*

- **`Reference`** identifies a *definition* — an element of a term component or a decl component.
- **`Referent`** identifies *what a term-position name can mean* — a term, or a data/effect constructor.

The point is that `Maybe.Just` is a term-level name with **no term hash of its own**. It is `Con (ConstructorReference #<declhash> 1) Data` — a reference to the declaring type plus an ordinal (`unison-core/src/Unison/ConstructorReference.hs:23-25`). Constructors are not separately addressable, have no dependents of their own, and cannot change independently of their type; `docs/branchless.md:11` spells out the consequence for the dependents index. Hence the asymmetry in the branch type: `terms :: Map NameSegment (Map Referent _)` but `types :: Map NameSegment (Map Reference _)`.

**Observation for arbor.** arbor has no analogue and will need one. The moment a language in the substrate gets datatypes with constructors — or p16/p17's record fields viewed as projections — there will be names that denote something addressable only *relative to* another definition. p16's label sort is a different answer to an adjacent question (field identity decoupled from field name, labels minted as their own sort), but a label is a first-class stored definition with its own hash, whereas a Unison constructor deliberately is not. Which way arbor goes for constructors is unasked; it should be. Filed for `../../design/open-questions.md`.

## Suffix resolution, and the rule arbor does not have

`Name` is a position (`Absolute`/`Relative`) plus a `NonEmpty NameSegment` stored **reversed**, last segment first. That is load-bearing rather than incidental: `suffixifyByName`'s doc notes it *"Only works if the `Ord` instance for `Name` orders based on `Name.reverseSegments`"* (`unison-core/src/Unison/Name.hs:603`), which turns suffix search into a range scan.

```
>>> suffixes "a.b.c"  ==>  ["a.b.c", "a.b", "c"]
```
`Name.hs:535-546`. Three strategies, all of the form `fromMaybe fqn (List.find isOk (suffixes fqn))`:

- **`suffixifyByName`** (`:604-619`) — shortest suffix matched by exactly one *name*.
- **`suffixifyByHash`** (`:630-648`) — shortest suffix whose matching *reference set* equals the FQN's. Weaker, therefore shorter: aliases do not create ambiguity, because they denote the same thing.
- **`suffixifyByHashName`** (`:653-674`) — by-hash, but keeps extending while the suffix could hit a non-`lib` definition, *"because such definitions could end up being edited in a scratch file, where 'suffixify by hash' doesn't work."*

That last caveat is not pedantry; `04-caching-update-merge.md` §"Update" shows the whole `update` algorithm turning on the difference between file context and codebase context.

**And a rule arbor lacks entirely: dependency depth breaks ties.** `Name.hs:597-599`:

> if there are two names `lib.base.List.map` and `lib.something.lib.base.Set.map`, then `map` would unambiguously refer to `lib.base.List.map`.

Implemented via `NamePriority`/`classifyNameLocation`/`nameLocationPriority`. Names nested more deeply under `lib` lose. The same one-level-of-`lib` rule governs deep traversal: `deepChildrenHelper` descends when `libDepth <= 1` **or the namespace hash has not been seen before** (`parser-typechecker/src/Unison/Codebase/Branch/Type.hs:343-361`) — note that hash equality is used to memoize away repeated subtrees, a direct dividend of Merkle addressing.

**Correction owed to arbor.** `../../design/04-naming-layer.md:160` lists as a non-goal: *"Suffix-based name disambiguation (Unison's `f.h1a2b3` convention). Our namespace is unambiguous by construction."* Two problems. First, `f.h1a2b3` is **hash-qualification**, a different mechanism from suffix resolution — and second, arbor *does* implement suffix resolution: p9, p10, and p17 all use Unison-style longest-suffix lookup with an `Ambiguous` error, described that way in `CLAUDE.md` itself. The non-goal and the prototypes disagree. Corrected in place; see `07-arbor-mapping.md` §"Corrections".

Separately, the `lib`-depth priority rule is a good idea arbor has no equivalent of, and it becomes relevant the moment a namespace contains vendored dependencies.

## `Causal` — history as a hash-consed DAG

```haskell
data Causal m hc he pe e = Causal
  { causalHash :: hc, valueHash :: he
  , parents :: Map hc (m (Causal m hc he pe pe)), value :: m e }
```
`codebase2/codebase/U/Codebase/Causal.hs:14-19`. Equality is by `causalHash` alone (`:22-23`); parents are monadic thunks, so history loads lazily.

Two hashes per node, kept as distinct newtypes (`codebase2/core/U/Codebase/HashTags.hs:8-25`): **`CausalHash`** identifies the history node, **`BranchHash`** identifies the namespace content. The causal hash is computed over the value hash plus the parent set:

```haskell
data Causal = Causal { branchHash :: Hash, parents :: Set Hash }
instance H.Tokenizable Causal where
  tokens c = H.tokens $ branchHash c : Set.toList (parents c)
```
`unison-hashing-v2/src/Unison/Hashing/V2/Causal.hs:12-18`. **Parents are a `Set`**, so a merge is commutative in its parents — merging A into B and B into A produce the same causal hash.

The V1 form is a three-way sum, `UnsafeOne | UnsafeCons | UnsafeMerge` (`parser-typechecker/src/Unison/Codebase/Causal/Type.hs:52-70`), and it comes with an **algebraic specification written out in the source** (`:24-42`) — five operations with their laws:

> - `before : Causal m a -> Causal m a -> m Bool` defines a partial order on `Causal`.
> - `head : Causal m a -> a`, the "latest" value in a chain.
> - `one : a -> Causal m a`, satisfying `head (one hd) == hd`
> - `cons : a -> Causal a -> Causal a`, satisfying `head (cons hd tl) == hd` and `before tl (cons hd tl)`.
> - `merge : CommutativeSemigroup a => Causal a -> Causal a -> Causal a`, **commutative (but not associative)**, satisfying `before c1 (merge c1 c2)` and `before c2 (merge c1 c2)`.
> - `sequence c1 c2 = cons (head c2) (merge c1 c2)`.

`lca` is a bidirectional breadth-first search over `causal_parent` (`Causal/Type.hs:92-104`); the schema notes that *"LCA computations only need to look at this table"* (`sql/create.sql:101-102`). A squash produces a plain `cons` onto the target rather than a merge node (`Unison/Codebase/Causal.hs:74-120`), memoized in `squash_results` keyed by the unsquashed branch hash (`sql/009`).

One design note in the schema is worth quoting because arbor will face it: *"`causal` references value hash **ids** instead of value ids, in case you want to be able to drop values and keep just the causal spine"* (`sql/create.sql:86-87`). History can outlive the content it points at.

**Observation for arbor.** `formalism/open-questions.md` §"The 'work up' ladder" lists **Branching / merging** as an unattacked rung. This is a ready-made target: a five-operation algebraic spec, already written down by its implementors, with laws, a commutativity property that falls out of using a set for parents, and an explicit non-associativity caveat. It is also strictly more structure than arbor's `H` — which is per-*name*, flat, and append-only, and cannot express "these two states have a common ancestor." See `07-arbor-mapping.md` §"History".

## Primary state vs. derived indices

Worth separating, because arbor's `../../design/02-definitions-and-derived-data.md` draws the same line and `../../related-work/01-content-addressing.md` §"Gaps" notes that *"nobody has published the primary-store-vs-derived-index argument."*

**Primary** (canonical, hash-bearing): `hash`, `text`, `object`, `hash_object`, `causal`, `causal_parent`.

**Derived** (rebuildable from the objects): `dependents_index`, `find_type_index`, `find_type_mentions_index`, `scoped_term_name_lookup`/`scoped_type_name_lookup`/`name_lookups`, `namespace_statistics`, `squash_results`.

The derived tables are treated as genuinely optional. `namespace_statistics` carries a warning in its migration that rows may simply be absent — *"computed on-demand … you should NOT perform queries or joins on this table which expect a row to exist for every namespace"* (`sql/003`) — and `NamespaceStats` is documented as static per hash (`codebase2/codebase/U/Codebase/Branch/Type.hs:65-73`), i.e. safe to cache forever for the same reason arbor's `E` is.

The **dependents index** is the reverse-dependency table (`sql/create.sql:209-232`), written during `saveTermComponent` (`Queries.hs:2578-2623`) — note `:2597`, which indexes intra-component self-references too. Its read API carries the subtlety that recursion forces:

```haskell
data DependentsSelector = IncludeAllDependents | ExcludeSelf | ExcludeOwnComponent
```
`Queries.hs:1741-1747`. arbor's `Store.callers_of` (p11) is the same primitive without the component wrinkle — because arbor has no components (`02-store-and-hashing.md` §"Components"), it has no need for `ExcludeOwnComponent`.

**And one derived index was deleted outright.** The `scoped_*_name_lookup` tables are still in `create.sql` but are vestigial: grepping the current tree for `scoped_term_name_lookup` hits only migration files `[verified by absence — rg scoped_term_name_lookup --type hs @ db60ce2]`. Names are now derived in memory from the branch (`Branch0._deepTerms`/`_deepTypes`). See `05-deletions.md` §"The name index".
