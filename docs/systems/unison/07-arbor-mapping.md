# Unison — Mapped onto arbor's formal objects

**Status:** Source read @ `db60ce2`. See `00-overview.md` for the pin and scope. This is the payload file: what the read means for `formalism/` and `docs/design/`.

**Nothing here is a commitment.** Per `../00-index.md` §"Relation to …", findings are inputs; they graduate into `../../design/decisions.md` only when a decision is actually taken. What *did* land in this pass is a set of factual corrections (§"Corrections applied") and a set of questions filed in `formalism/open-questions.md`.

## The map

`formalism/paper/arbor-core.tex` models the configuration ⟨Σ, N, E, H⟩ — term store, namespace, evaluation cache, binding history — and `arbor-stlc.tex` adds Θ, the hash-valued `TypeOf` aspect. Each has a Unison counterpart, and every one of them differs in a way worth naming.

| arbor | Unison | The fork |
|---|---|---|
| `Σ : Hash ⇀ Node` | `Hash ⇀ Node⁺`; a definition is `(Hash, Pos)` | Mutual recursion changes the *type* of the store |
| `Ref(h)` opaque | `Ref(h)` hash-transparent | Inlining is identity-preserving there, not here |
| `Θ` derived, observational | type is *inside* the term hash | Signature change = new definition there |
| `N : Name ⇀ Hash`, exact | `Map NameSegment (Map Referent _)` + suffixes + `lib`-priority | Conflict is a representable state, not an error |
| `H` per-name, flat, append-only | `Causal` — hash-consed DAG, per-subtree, parents a set | arbor's `H` cannot express a common ancestor |
| `E : Hash ⇀ Hash`, write-once | `watch_result`, no invalidation, key need not be a definition | Same theorem; plus a side condition arbor lacks |
| `Migrate` = structural rewrite ρ | print → re-parse → re-typecheck | arbor has the lemma Unison's method needs |
| `clean` oracle; mints for lineage | synhash: hash modulo names | Provenance without a second identity axis |
| Mint (`10-minted-identity.md`) | `unique` GUID, default-on for types | Confirms per-sort; prices the upkeep |

### Σ — the store is keyed by a pair

`02-store-and-hashing.md` §"Components". A hash names a strongly-connected component; `Reference = (Hash, Pos)`.

arbor has deferred this. `../../design/03-content-addressing.md:141` files it as *"we'll need a canonical ordering for the group so the hash is stable. Unison's approach is a known starting point."* Two understatements, now correctable:

- **It is not just an ordering.** `Σ : Hash ⇀ Node` becomes `Σ : Hash ⇀ Node⁺`, and every reference becomes a pair. `wf`'s no-dangle clause, `callers_of`, the migration rewrite ρ, and `thm:alpha` all change shape. It is a rung on the ladder, not a canonicalizer tweak.
- **The starting point has a known defect.** `IncompleteElementOrderingError` (`ABT.hs:190-192`, issue #2787): when two cycle members are structurally identical modulo intra-cycle references, no canonical order exists and Unison refuses, asking the user to perturb one definition by hand. **Canonical form for an SCC is a partial function.** That is worth knowing before arbor promises a total one.

### `Ref` — hash-transparency, and what it is not

`02-store-and-hashing.md` §"Reference transparency". The precise fork: both systems store a `Ref` node; in Unison it hashes to the referent's hash, so **inlining is hash-preserving**. `formalism/decisions.md`'s rationale for reintroducing `Ref` — that the callers metatheory is vacuous without indirection — is untouched, because Unison keeps the indirection too.

What is actually at stake is whether the *factoring* the author chose is part of identity. arbor says yes; Unison says no. `../../design/12-type-abstraction.md`'s projection-inlining dependency model needs arbor's answer, since its whole point is that inlining a projection changes what a definition depends on. Filed as a modeling question, not a defect.

### Θ — type as aspect vs. type in the hash

`02-store-and-hashing.md` §"The type is inside the term hash". `hashTermComponents` wraps every term in `TermAnn e typ`; `Convert.hs:87` warns against the type-free variant for storage.

`arbor-stlc.tex`'s headline posture is that **nothing is gated on types** — store and namespace stay permissive, typing is observational, Θ is a derived aspect keyed by hash. Unison's choice is the clean opposite, and each buys something specific:

- **Type-in-hash** — a cached type can never be stale, because a type change is an identity change. Combined with `checkCacheability` (below) the type is doing identity, caching, and effect-tracking at once.
- **Type-as-Θ** — the store can hold ill-typed and partially-typed terms. That is not a nicety; it is a *requirement* of p8/p9's holes line, `Type_with_holes`, and the whole `feedback_visible_breakage` posture of commit-and-report.

The point for the paper is that arbor-stlc's choice currently reads as the obvious one. It is not; it is the one the holes line forces. Worth one sentence saying so.

### N — exact function vs. relation with priority

`03-namespace-and-history.md` §"Names map to sets" and §"Suffix resolution".

`formalism/decisions.md` (2026-07-23, "Naming modeled at the substrate primitive") already carries an *"Honest divergence from the prototypes"* note about exact resolution. Unison supplies the mature endpoint: names are a relation; conflict is a legal state; **unconflictedness is a derived view that operations demand when they need it** (`asUnconflicted :: Either Conflicted Unconflicted`, recomputed on every setter). That is the same commit-and-report discipline arbor applies to types, applied to names — and arbor applies the opposite discipline to names by making `N` a partial function.

Separately, the `lib`-depth priority rule (`Name.hs:597-599`) is a naming-layer idea arbor has no equivalent of and will want once namespaces contain vendored dependencies.

### H — flat per-name log vs. hash-consed DAG

`03-namespace-and-history.md` §"Causal".

arbor's `H` is append-only *per name*: bind/rebind/unbind, orphans rendered `name(vN)`. It answers "what was this name bound to before?" It cannot answer "do these two states share an ancestor?", which is the question every merge asks.

Unison's `Causal` is the shape that can, and it arrives with an algebraic specification its implementors wrote out (`Causal/Type.hs:24-42`): five operations, `before` a partial order, `merge` commutative-but-not-associative, `sequence` derived. Two structural facts make it work — parents are a **set**, so merge is commutative in its parents by construction; and `children` maps to a child *causal*, so every subtree has independent history that the parent pins.

`formalism/open-questions.md` §"The 'work up' ladder" lists **Branching / merging** as an unattacked rung with no target. It now has one, and a specification to be faithful to rather than invent.

### E — the theorem, running

`04-caching-update-merge.md` §"The cache". This is the closest match in the whole read, and it is close enough to be evidence.

- **No invalidation logic exists.** `[verified by absence @ db60ce2]`, search recorded. Stale entries are unreachable, not wrong, because the key transitively includes every dependency. That is `thm:stability` + `cor:cache`.
- **The key need not be a named definition** (`sql/create.sql:141-143`). That is `formalism/decisions.md`'s no-definition-sort stance (2026-07-30), reached independently and for the same reason.
- **Results are shareable**, and Unison relies on it: test results sync with the codebase, so "is this branch passing" becomes a set intersection (`docs/testing.markdown`). arbor proves stability under *store growth*; the commons argument needs stability under *transfer*, which is the same theorem with a different quantifier. Worth stating as a corollary.

**And one thing arbor's `E` does not have:** `checkCacheability` — cacheable iff the type contains no arrows, *"since top-level definitions can't have effects without a delay"* (`Runtime/Interface.hs:463-475`), plus `RunWatch` never cached. arbor's `E` is total because the language is pure and a value is just another closed term. The moment either changes, a side condition appears. Filed.

### Migration — and the lemma arbor already has

`04-caching-update-merge.md` §"Update". Unison's `update` renders affected dependents back to source, re-parses, and re-typechecks — *"the world's weirdest implementation of AST substitution"* (`Update2.hs:424-427`).

**That is only correct if print-then-parse preserves hashes, and Unison does not state the property — it tests it**, over a regression corpus, with a transcript asserting the namespace diff is empty (`unison-src/transcripts-round-trip/`, `reparses-with-same-hash.u`).

**arbor proves it.** `arbor-core.tex` Part III makes printing a judgment and establishes `prop:print-elab` (exact for every printer choice) and `prop:elab-print` (up to the kernel of elaboration) — per `formalism/decisions.md` 2026-07-30, replacing an earlier vaguer statement.

This is the sharpest positioning claim the read produces, and it is better than the general "first formal treatment" framing at `../../related-work/00-index.md:102` because it names a specific mechanism in a specific shipping system that the theorem covers. Two honest caveats before it is used: arbor's propositions are stated for its own core and surface languages, not Unison's, so this is an argument about the *shape* of the guarantee rather than a theorem about Unison; and arbor's own `Migrate` is a structural rewrite ρ that needs no round-trip lemma at all, so arbor proves something it does not itself depend on.

The fork is also worth keeping: printing through text costs a round-trip obligation and buys the property that what the user is shown is exactly what was re-typechecked.

**How the dependents are chosen, and how that maps to pin/follow/explicit,** is worked through in `08-merge-and-branching.md` §"How update picks dependents". The short version: `transitiveDependentsWithinScope` takes *every* transitive dependent that has an unconflicted name in the branch, with **no type-based gating** — Unison used to classify edits as `Same | Subtype | Different` and stopped. So Unison ships arbor's **Follow** with scope fixed at the whole namespace, and neither **pin** nor **explicit** exists. arbor's 2-axis framing is richer than the closest system's; Unison's *failure* handling is better than arbor's, because the residual comes back as editable source rather than as a set of names.

### Provenance — synhash vs. mints

`04-caching-update-merge.md` §"Merge". Unison answers "human edit or auto-propagation?" by hashing **after substituting names from a pretty-print environment** — quotient out the dependencies' identities and see whether what remains is the same.

That is a real alternative to a second identity axis. p11's mint threads carry a mark forward through edits; synhash recovers the same information after the fact, with no extra state to keep stable. Their failure modes are complementary, which is what makes the pair informative: synhash's own header says it *"cannot handle renames very well"*, because its quotient is by name; mint threads survive renames by construction but need machinery to survive merges (below).

### Mint — confirmed, and priced

`04-caching-update-merge.md` §"Minting".

**Confirmed, and upgraded from inference to primary evidence.** `DeclParser.hs:230` — a declaration with no modifier calls `resolveUniqueTypeGuid`. Unison is unique-by-default for types, `structural` is the marked opt-out, and the GUID enters the hash as `[Tag 1, Text guid]`. This closes the gap at `../../related-work/01-content-addressing.md` §"Gaps" and answers `../../design/open-questions.md:77`. Per-sort minting — types yes, terms no — is confirmed as `../../design/10-minted-identity.md:93` describes.

**Priced.** A mint must survive re-saving, and Unison has no explicit *edit-of-X* gesture, so it reconstructs the intent **by name** at save time. `UniqueTypeGuidLookup.hs:19-20`: *"If there are multiple such types, an arbitrary one is chosen."* Plus a `namespace_unique_type_guid` table (`sql/016`) and `makeUniqueTypeGuids` to keep GUIDs from diverging under merge.

Two readings, and they pull in opposite directions, which is the useful part:

- **For arbor's design.** p11's explicit *edit-of-X* gesture is exactly what removes the guesswork. Unison pays a rename hazard and an arbitrary tiebreak for not having one. First external evidence in favour of arbor's choice.
- **Against arbor's completeness.** The reason Unison needed `namespace_unique_type_guid` is *merge* — two branches minting independently for the same name. p11's threads have that problem too and no answer, and `formalism/open-questions.md`'s mint/thread rung will have to handle it.

## The finding that does not fit the table

`05-deletions.md`. Unison has removed three reified side-structures — metadata links (2024), patches and propagate (2025), the SQL name index (2025) — replacing each with derivation-on-demand or naming convention. arbor is currently building all three categories.

The narrow, defensible version of the reading:

- **Aspects.** The metadata mechanism Unison deleted is `../../design/02-definitions-and-derived-data.md`, proposed independently. What survived there is precisely the *derived, hash-keyed, reverse-queried* half — the evaluation cache and the type index — while the *asserted, name-shaped* half (docs, authors, license) lost to `foo.doc`. arbor's asserted/derived split already names this line; the finding is that the line is load-bearing rather than descriptive.
- **Reified edits.** `Patch` is what `H` becomes if it ever gains the ability to be *applied*. Unison tried that and abandoned it for LCA-diff plus re-typecheck. arbor's `H` is on the safe side of that line today; worth staying there deliberately rather than by accident.
- **Derived indices.** Unison deleted its *forward*-derivable index (names, recomputable by memoized traversal over subtree hashes) and kept its *reverse* one (dependents, not recomputable by traversal). arbor's `Store.callers_of` is the reverse kind. The split is a point in favour of arbor's primary-store posture at `../../related-work/01-content-addressing.md` §"Gaps", and it is sharper than the general argument because it says *which* indices survive.

## Corrections applied in this pass

Factual claims about Unison only. No arbor stance, decision, or non-goal was changed, and `../../design/decisions.md` and `formalism/decisions.md` are untouched.

| Where | Was | Now |
|---|---|---|
| `../../design/03-content-addressing.md` §"Unison as prior art" | "Automatic migration UX (`update`, `patch`, etc.)" | `patch`/`propagate` deleted 2025-05-10; describes the diff + dependents-index + re-typecheck mechanism that actually exists. Decline unchanged. |
| `../../design/03-content-addressing.md` §"Open sub-questions" | "Unison's approach is a known starting point" | Adds that it changes the store's type, and that canonical SCC ordering is partial (#2787) |
| `../../design/03-content-addressing.md` §"Hash representation" | "we accept full state rebuilds" | Adds Unison's versioned-hash + `hash_object` indirection as prior art. Stance unchanged. |
| `../../design/10-minted-identity.md` §"Prior art" | "on the evidence of its own pretty-printer output, though the docs never say so" | Source-verified at `DeclParser.hs:230`; adds the upkeep cost |
| `../../design/04-naming-layer.md` §"Non-goals" | "Suffix-based name disambiguation (Unison's `f.h1a2b3` convention)" | Separates hash-qualification from suffix resolution, and notes that p9/p10/p17 *do* implement the latter |
| `../../related-work/01-content-addressing.md` §"Unison", §"Minting" | Inference-tagged claims | Retargeted here; the `unique`-default inference upgraded to primary evidence |
| `../../related-work/00-index.md` | "no publication exists" as the whole story | Unchanged as a finding; annotated with the source read and the pin |

## Questions filed, not answered

In `formalism/open-questions.md` §"Divergences from Unison (source read 2026-08-07)": the store's key type; hash-transparent vs. opaque `Ref`; type-in-hash vs. Θ; `H` vs. `Causal` as the branching rung's target; migration by ρ vs. by print-and-reparse; synhash as an alternative to mints; whether `E` needs a `checkCacheability`-shaped side condition; and whether stability-under-transfer deserves its own corollary.

In `../../design/open-questions.md`: whether the asserted/derived aspect split should be load-bearing; constructors as names without hashes; the `lib`-depth resolution rule; mint stability under merge.

In `../../design/03-content-addressing.md` §"Mutual recursion: three ways to close the cycle": alternatives to Option A, written up because this file's §Σ entry made it obvious the first one had been treated as the only one. Closing the cycle with a **binder in the object language** — a `Fix` over a product, members as projections — leaves `Σ : Hash ⇀ Node` untouched and costs only a `Fix` node. The ordering ambiguity does *not* vanish, but its consequence changes: with a **positional** tuple the bundle's hash is invariant under group automorphisms, so the case that makes Unison refuse leaves only an arbitrary, behaviour-preserving choice of which projection carries which name — a coincidental non-collapse rather than a rejected ingest. A **labeled** record instead resolves it with minted labels, which restores presentation-independence but relocates the question to name-keyed mint stability — the same mechanism as `loadUniqueTypeGuid` (§"Mint" above). Not chosen; the point is that the choice exists, and that the real fork is a mint versus a rare loss of presentation-independence.

**The running list of everything this read opened and did not close is `10-followups.md`.**
