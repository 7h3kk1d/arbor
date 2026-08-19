# Formalism decisions

Running log of modeling commitments for the arbor formalism. Lightweight ADR: each entry has a
date, the decision, a rationale, and noted alternatives. When a decision is reversed, add a new
dated entry rather than editing the old one. This log concerns *how we formalize*; substrate
design commitments live in `docs/design/decisions.md`.

---

## 2026-07-23 — First formal artifact: `arbor-core` over untyped λ

The formalism begins with a single LaTeX document, `arbor-core`, over the untyped λ-calculus
(the p4 fragment), covering the term store, the namespace, a derived evaluation cache, and an
edit calculus. Successors work up the same TAPL ladder the prototypes follow (types next).

**Rationale.** Mirrors the prototype method (`docs/design/decisions.md` 2026-04-21, "disposable
experiments") applied to proof artifacts: the smallest language that still exhibits
content-addressing's core payoff. Untyped λ is enough to state and prove no-silent-breakage and
evaluation stability; types are additive later.

**Alternatives.** Start at STLC (p6) — rejected: types obscure the two headline theorems, which
are about *identity and naming*, not typing. Formalize the whole p17 stack at once — rejected on
the same "step in slowly" grounds the prototypes use.

---

## 2026-07-23 — Abstract hash + indirect `Ref` (generalizing p4)

`Hash` is modeled as an opaque type equipped with an injective `hash : Node → Hash` (a stated
axiom in LaTeX; a single postulate in Agda). Stored terms reference callees through a
**`Ref(hash)` node** resolved against the store. This **reintroduces the `Ref` constructor that
p4 deliberately elided** — p4 inlines closed subterms at resolve time (`resolver.re`,
`canonicalize.re`) and has no `Ref`.

**Rationale.** The interesting metatheory (the reverse-dependency / callers relation, explicit
migration, immutability of callers under edits) is *vacuous* without indirection: if references
are inlined there are no cross-definition edges to break or rewrite. `docs/design/03` mandates
`Ref(hash)` ("every cross-definition reference … is a hash reference — `Ref(hash)` — never a
name"); p4 is the restricted, single-checkout instance where inlining suffices. The formalism
models the substrate ideal and cites p4 as the special case.

**Injectivity as idealization.** Cryptographic collision-resistance is modeled as exact
injectivity (`docs/design/03`: "collisions are treated as impossible in practice"). The concrete
BLAKE2B + tag-byte encoding (`node.re`) is presented as *one realization* of the abstract
`hash`, not part of the abstract theory.

**Alternatives.** `Hash := canonical term` (interning), which makes injectivity hold by
`refl` and needs zero postulates — rejected for v1 because it collapses `Ref` back to inlining
and makes the store semantically inert, weakening exactly the theorems we want. Recorded as the
postulate-free fallback in `agda/README.md`.

---

## 2026-07-23 — Scope: migration without mints; well-formedness gate

`arbor-core` includes the full edit calculus — `bind`/`rebind`/`unbind`, append-only binding
history with `name(vN)` orphan rendering, the `callers_of`/`callers_closure` reverse-DAG,
atomic `multi_rebind`, and pin/follow/explicit as points in the `scope × user-in-loop` space
(`docs/design/04` §"Update strategies"). It **defers** the mint/thread second identity axis
(p10/p11) and **all types**.

**Consequence — the gate.** p11's cascade is gated by a real *typecheck* (`update_strategy.re`
calls `Typecheck.check_top`; p11 is typed via the p9 line). Untyped, this becomes a
**well-formedness gate** (closed + all refs resolve). Because substituting one closed `Ref` for
another preserves closedness and resolvability, the gate is *near-total*: `follow` essentially
always succeeds. This is called out in the paper as a feature, not a gap — it shows the
interesting failure modes of `follow` are exactly what *types* introduce, motivating the sequel.

**Consequence — lineage without mints.** p11 uses the mint to say "these hashes are versions of
one thing" independent of any name. `arbor-core` recovers version lineage more weakly, from the
per-name binding **history** `H` (the sequence of hashes a name has held). Mint-as-identity is a
documented successor.

**Rationale.** The user asked for updates/explicit migration in the first artifact, but "start
small" argues against pulling in a whole second identity axis. History-based lineage is enough
to state migration correctness and orphan rendering.

**Alternatives.** Minimal (store + naming only, migration deferred) — rejected: the user named
migration explicitly. Full p11 including mints + the `follow-clean` oracle — deferred as a
successor.

---

## 2026-07-23 — Evaluation cache is a first-class derived aspect

The formalism models the evaluation cache `E : Hash ⇀ Value` as a **derived** aspect
(immutable once written), placed beside the **asserted** namespace `N` in arbor's Attachment
layer. This directly answers the design goal that *already-evaluated terms are never
re-evaluated*.

**Rationale.** It makes the Attachment layer appear with its two real instances (names + eval
results, per `docs/design/02` and p4 `attachment.re`) and lets us prove the dual of
no-silent-breakage: cache soundness under store growth, and migration incrementality (only new
hashes are evaluated). It also formally explains p4's rule that `StepLimit` is never cached
(`eval.re`): only *defined* evaluations are stable, so only they are cacheable.

**Alternatives.** Leave the cache out and mention re-evaluation informally — rejected; the
"don't re-evaluate" property is one of the two theorems that justify the whole design.

---

## 2026-07-23 — Evaluation is a relation; the step budget is an implementation

Evaluation is modeled as a partial relation `Σ ⊢ t ⇓ v` (CBV, weak-head normal form; undefined
on divergence). p4's `default_step_limit = 10000` budget and its `Value | Stuck | StepLimit`
result (`eval.re`) are treated as an *implementation* of this partial function: `Value`/`Stuck`
are the defined cases; `StepLimit` marks "not yet known to be defined."

**Rationale.** A relation is the right object for metatheory (stability, determinism), and it
makes T5/T6 clean: cacheable ⟺ defined. The Agda bridge is a fuel-indexed
`eval : ℕ → Σ → Term → Result` with a fuel-monotonicity lemma (`eval_k = Value v ⟹ eval_{k+1} =
Value v`), which is exactly why `StepLimit` must not be cached but `Value`/`Stuck` may.

**Alternatives.** Formalize the budget directly — rejected: budget-relative results are not
stable under increasing budget, so caching them (by hash alone) is unsound; the relation makes
this precise instead of baking a magic constant into the theory.

---

## 2026-07-23 — Naming modeled at the substrate primitive (exact resolution)

Resolution is modeled as the substrate primitive `N : Name ⇀ Hash` with exact lookup and
exactly two outcomes (Found / NotFound). `Name` is an opaque string; the substrate imposes no
hierarchy and no ambiguity (`docs/design/04:22-33`, which lists suffix resolution as a
non-goal).

**Honest divergence from the prototypes.** p9/p11 *do* implement, at the **editing layer**, a
`resolve_query` with dotted-segment longest-suffix matching and a distinct `Ambiguous` error
(`namespace.re:283-334`), alongside the exact `resolve` (`namespace.re:260`). The formalism
takes the exact `resolve` as the substrate primitive and treats suffix resolution as a definable
editing-layer elaboration over it, deferred to a successor. This matches `04`'s "names are an
editing-layer concern" split and keeps the first artifact's resolution judgment two-valued.

Prototype-level guards not lifted to the formalism: `bind`/`rebind` reject reserved keyword
names (`namespace.re:29-57`) — a surface-grammar UX guard, not a substrate property (substrate
names are opaque strings).

**Alternatives.** Model `resolve_query` + `Ambiguous` as the substrate resolution — rejected:
it is editing-layer per `04`, and a three-valued resolution complicates the elaboration
round-trip without touching the headline theorems.

---

## 2026-07-30 — Definition roots, guarded transitions, ref-acyclicity (first review pass)

A design review of the draft (before starting any successor artifact) found three interlocking
gaps, now fixed in the paper. They share one root cause: dropping p11's `Named` wrapper sort
left "definition" informal, and several statements quietly leaned on it.

**1. Rooted store.** A store is now a pair `(Σ, R)` with `R ⊆ dom(Σ)` the **definition roots**.
Previously `wf` was not actually a predicate on `Σ` (its closedness clause said "bound in `N`",
crossing into the namespace), and `callers`/the cascade quantified over all of `dom(Σ)` — which,
storage being shallow, sweeps in every anonymous open subterm node (`Var 1` under a `Lam` is an
entry). `Ingest`/`Migrate` populate `R`; `wf`, `callers`, and the cascade are parameterized by
it. p11 realizes `R` as the `Named_term`/`Named_type` sort (minus the mint); p4 leaves it
implicit.

**2. Guarded transitions + configuration coherence.** `Ingest` gains premises `closed(t)` and
`refs(t) ⊆ R` — previously it had none, so ingesting `ref h₀` at a dangling `h₀` falsified
wf-preservation as stated; the old proof sketch appealed to "edit-time resolution", an
editing-layer fact that is now an actual lemma (elaboration produces closed terms with
`refs ⊆ ran(N)`) feeding actual premises. `Bind`/`Rebind` gain `h ∈ R`: the namespace layer
alone cannot check this (p11's `namespace.re` accepts any hash — validation happens one level
up, in `multi_rebind`'s `target_exists` and the editor's resolve step); a config-level
transition can state it directly. `Rebind` stays total in the name, faithful to p11's `rebind`
(which also binds fresh names). New **coherence** invariant on configurations: `wf(Σ,R)`,
`ran(N) ⊆ R`, history `Some`-hashes ⊆ `R`, history coherence, `Cache-sound`; preservation is
now the T4 statement.

**3. Ref-acyclicity as a `wf` clause.** Injectivity (★) alone is consistent with hash fixpoints
(`hash(Ref h) = h`), and the old `wf` did not exclude reference cycles: `reconstruct` stops at
`Ref` leaves, so only *structural* cycles were ruled out. A cyclic reference graph makes
`callers*` cyclic, so migration's "dependency order" — previously used but never defined — need
not exist. New `wf` clause (iv): the reference graph is acyclic. Dependency order is now defined
(a topological sort of the in-scope callers closure, existence from (iv)), and the cascade
result is proved order-independent (new lemma). The lemma matters because p11's
`callers_closure` returns DFS *discovery* order, not dependency order — harmless in p11, whose
references are deep structural containment substituted against a body-keyed mapping, but
load-bearing here, where substitution rewrites shallow `Ref` leaves.

**Alternatives.** (a) Derive the root set from the configuration (`ran(N)` ∪ ref-targets ∪
history hashes) instead of carrying `R` — rejected: it leaves `Ingest` without a meaning
("register a definition" is exactly what adds a root) and makes `wf`'s parameterization awkward
to mechanize. (b) Keep `wf` weaker and prove acyclicity only as an invariant of reachable
configurations — rejected: `wf` should be self-contained (a store either supports migration or
does not), and preservation is provable anyway from `Ingest`'s new premises.

---

## 2026-07-30 — No definition sort: names confer definition-hood; migration is a whole-store rewrite

Supersedes the "rooted store" commitment of the previous entry (same day), after working the
design through concrete cascade examples. The guarded-transitions and ref-acyclicity
commitments stand, reworked to fit.

**The stance.** In a mint-free artifact there is no "unit of intent": a name is a label used to
match λ-terms in the store, and hash-equality is the only identity. So (a) the store carries
**no definition sort** — an explicit root set `R` is provenance identity, i.e. a degenerate
mint, and p11's `Named` wrapper (which the previous entry cited as precedent) carries a mint
for exactly this reason; (b) **naming a closed stored term is what treats it as defined** —
`Bind`/`Rebind` require only `closed_Σ(h)`, so any closed stored hash may be named, including
one that only ever existed as a subterm; (c) **migration propagates structurally**: updating
`Z → Z'` rewrites every store entry reaching `Z` (scope-guarded at reference crossings and
name bindings), and names simply follow their hashes. Unit-scoped following returns with the
mint successor.

**What changed in the paper.** `wf(Σ)` is again a predicate on the store alone: (i) reconstruct
defined, (ii) reference targets closed (directly the E-Ref soundness condition; not circular —
a target's closedness ignores the target's own refs), (iii) reference graph acyclic. Config
back to `⟨Σ, N, E, H⟩`; coherence requires name and history hashes closed. The cascade is
replaced by a **whole-store rewrite `ρ`**, defined by recursion on the combined
structural+reference graph (acyclic by (i)+(iii); mixed cycles project to reference cycles) —
order-free by construction, so the dependency-order machinery survives only as the
sequential-pass lemma bridging to p11's implementation. Strategies become propagation scopes
(`Pin ↦ ∅`, `Follow ↦ dom(Σ)`, `Explicit(S) ↦ S`) consulted at reference crossings and at name
bindings. Two faithfulness dividends: under Follow, aliases of the edited definition now
rebind (p11 does this too, via its wrapper-as-caller path; the pre-review `U` missed them),
and under Pin only the edited name moves, as in p11.

**Findings worth keeping.**
- The untyped gate is *total*, not just near-total: rewriting preserves closedness and
  registers referents before referrers, so `Follow` cannot fail and migration atomicity is
  vacuous here. The contract is stated anyway — it is what the typed successor's gate needs.
- Alternatives visited and rejected on the way: **bare closedness as the definition sort**
  (every `Ref` stub and anonymous closed fragment becomes a "definition"; callers/cascade
  bookkeeping fills with junk pairs) and **"ever named" (`ran(N)` ∪ history) as cascade scope**
  (Follow silently under-propagates through orphaned intermediates: the rewritten orphan is
  never named, so a *second* migration pins at it — lineage through unnamed intermediates is
  mint-shaped state). The whole-store rewrite has neither problem; its cost is that "what was
  migrated" is characterized structurally (`ρ(h) ≠ h`) rather than as a list of user-facing
  units — the honest cost, since units do not exist at this rung.

---

## 2026-07-30 — arbor-stlc: content-addressed types, commit-and-report migration, the clean oracle

Second formalism artifact, `paper/arbor-stlc.tex` — the STLC rung, written as a delta
document over arbor-core (which is unchanged; cross-references via `xr-hyper` with a `core:`
prefix, every external reference rendered "arbor-core Definition N.M").

**Content-addressed types** (user decision), following p9's extension and diverging from p6,
which stores annotations inline in `Lam` nodes: the store becomes two-sorted (term entries +
type entries `TBool`/`TArrow(h,h)`), `Lam` carries a type hash, and structural type equality
collapses to hash comparison. Sub-decision: type entries are *shallow* where p9's are deep
(`Definition.Type(Ty.t)` stores whole types) — the store's uniform discipline; equivalent up
to interning since types are finite and ref-free. Sort discipline folds into
reconstruct-definedness (no new wf clause). Rejected alternative: p6-style inline annotations
— makes types second-class and blocks the hash-equality/TypeOf story.

**T-Ref is new formal content.** p6 has no reference rule — it inlines referenced bodies at
resolution, so no reference survives to `infer`. The formalism types references through the
store (`Σ;∅ ⊢ reconstruct(Σ,h) : T ⟹ Σ;Γ ⊢ ref h : T`), the same idealization step as
arbor-core's reintroduction of `Ref`. The judgment is deliberately pure of the Θ aspect,
mirroring ⇓ never reading E.

**Fully permissive store AND namespace; typing is observational.** No transition gains a
typing premise: ill-typed closed terms may be stored *and named* (a legitimate mid-refactor
state). The substrate computes and reports typing — `typeof`, the hash-valued Θ aspect
(p9-style; p6's inline `Type_of(Ty.t)` recorded as divergence), the standing `broken(Σ,N)`
query, and the migration residual — but never gates on it. p6's resolver typecheck is
editing-layer policy above these observations. (User direction: breakage visible and
queryable, never forbidden.)

**Migration = commit-and-report, diverging from p11's abort.** Migrate commits exactly as in
arbor-core; the typed rung adds its **report**: `residual` = the followers (name-level:
rebound names) that were well-typed before and are not after — the "continue the refactor"
list. p11's `Cascade_typecheck_failed` abort is recoverable as editing-layer policy ("if
clean then migrate else ask"). Headline theorems: **residual exactness** (broken′ =
(broken ∖ healed) ∪ residualN — a migration breaks exactly what it reports, never silently);
**type-preserving migration is total** (same `typeof` at the seed ⟹ empty residual under
every scope, zero rechecking — p11's `same_type` short-circuit promoted from a dead code
path to a theorem); **local soundness** (a well-typed term evaluates safely regardless of
ill-typed neighbors — its T-Ref closure carries its own support; p6's "Stuck is theoretically
unreachable… defence in depth" becomes the progress theorem).

**Finding: `clean` is not a stable aspect.** clean/residual are decidable *before* migrating
(the oracle and the report are definitionally aligned — the property p11's `dry_run` only
approximates: it checks `Named_term` callers only, ignores the strategy filter, and does not
accumulate the substitution map its own cascade builds). But clean is a property of the
*current* store: ingesting a new caller can falsify it. p11 caches `follow-clean:v1` as a
write-once bool keyed `BLAKE2B(h_old‖h_new)` — a key encoding nothing that grows with the
store — so a cached `true` silently goes stale and is never corrected: **unsound**. The one
soundly cacheable fact in its neighborhood is the type-preservation test itself (typing of a
stored hash is absolute). Sound keys for the general predicate → open-questions.

**Deferred:** type names in N / type aliasing, type-level migration (p11's `Named_type`
path), p6's second language + translation, mints, Agda mechanization of this rung.

---

## 2026-07-30 — Hash-valued cache; single identifier leaf; printing as a judgment

Three fixes closing the remaining faithfulness items from the review (5a, 5b) plus the
surface↔store round-trip formalization.

**Cache is store-to-store.** `E : Hash ⇀ Hash`: the cache maps a term's hash to the hash of its
value, *itself ingested into the store*. Both key and value are store terms; the cache never
touches surface syntax (the design requirement), and this matches p4 exactly — its cached
aspect is `Eval_value(Hash.t)` and its evaluator ingests every result and intermediate reduct
(`reconstruct → beta → ingest`). Consequence: the `Eval` transition now grows `Σ` (monotonicity
updated); `Cache-sound` reads through `reconstruct`. The `⊢ ⇓` relation itself stays term-level
and pure — the purity of evaluation in `(Σ, h)` is still what the headline theorems lean on;
the fueled hash-level `eval` that ingests is its implementation (Agda bridge updated).
Term-valued `E` was the rejected alternative: equivalent for the theorems, but it parks deep
terms outside the store, against the convention that derived aspects are hash-valued (p5
`Translation_target`, p9 `Type_of`).

**Single identifier leaf.** The draft's two-leaf surface (`sVar` for bound, `sName` for free)
was unfaithful — p4's `surface_ast.re` has one `Var(string)`; a parser cannot know which is
which — and its `El-Bound` was ambiguous under shadowing (`Γ(i) = x` admits any matching
index). Now: one leaf, `El-Bound` takes the *least* index (innermost binder wins), `El-Free`
requires `x ∉ Γ`; the rules are disjoint and elaboration is a partial function.

**Printing formalized, both round-trips stated.** Printing `Δ;N ⊢ t ⇚ s` is now a judgment,
deliberately *relational*: `P-Lam` picks any binder name, `P-Ref` any alias, and the leaf side
conditions (least index; `x ∉ Δ`) silently reject capturing choices. `pretty.re` is one
deterministic refinement (fresh-name supply, alphabetically-first alias). Round-trip A:
print-then-elaborate recovers the stored term exactly, for *every* choice the printer could
make. Round-trip B: elaborate-then-print holds up to `≈` = the kernel of elaboration,
characterized as generated by capture-avoiding binder renaming + alias swap. This closes the
"Round-trip precision" open question. Orphan forms `x(vN)` remain display-only (not surface
syntax); printing is undefined at a hash with no name and no history — a namelessness the
editing layer can always avoid.

---

## 2026-08-05 — Mechanization: statements as types, extrinsic scoping, relations for partial functions

First Agda code (`agda/`). Milestone 1 is proved; every other statement in
`arbor-core` is stated and marked open. Six modeling commitments, three of them
reversing or correcting what `agda/README.md` had recorded.

**Why now.** The Agda side had drifted badly from the papers: no code against
~37 stated theorems, `agda/README.md` untouched across the whole arbor-stlc
landing, arbor-stlc's own mechanization note deferring a reuse-boundary decision
that was never made, and both the README and this log still indexing theorems as
`T1`–`T8` — a numbering neither paper uses. Paper `\label`s are now the only
names, and `scripts/check-labels.sh` enforces it.

**1. Statements are types, not postulates.** Agda's `--safe` rejects postulates,
which turned out to be the right pressure. Each numbered result is a named type
in `Arbor/Core/Meta.agda` (`thm:nsb` ↦ `Thm-nsb`), *proved* exactly when that
type has an inhabitant. So the library assumes nothing — (★) is a field of the
`HashModel` record it is parameterized over, and `Arbor/Core/Model.agda`
exhibits an inhabitant, so no proved statement is vacuous. An open obligation
cannot be silently leaned on by a later proof, because there is no term to lean
on. Rejected alternative: postulate the open statements in a non-`--safe`
module, which would let M2 build on unproved lemmas but weakens what a green
build means.

**2. Extrinsic scoping — reversing the recorded lean.** `agda/README.md` and
`open-questions.md` both leaned intrinsic (`Term : ℕ → Set`, closedness as a
type). That collides with shallow storage: `dom(Σ)` contains every subterm ever
ingested, open ones included (`rem:nosort`), so `reconstruct` at `⌈nvar 5⌉`
yields an open term and an indexed `Term` would force its codomain to package a
level existential — one that is not even canonical, since `var 5` inhabits
`Term 6`, `Term 7`, … . Extrinsic also matches the paper, whose `def:closed`
already *is* an inductive relation and whose `Ingest` premise already hands
closedness over explicitly. α-equivalence stays definitional either way; it
comes from de Bruijn, not from the indexing, so `thm:alpha`'s (⇐) half is still
`cong`. Cost, accepted: `beta`-preserves-closedness becomes an explicit M2
obligation instead of a typing fact.

**3. The paper's partial functions become inductive relations.** `reconstruct`
is `σ ⊢ h ⇝ t` with a functionality lemma; the migration rewrite ρ will be the
same. As Agda functions they need termination arguments that do not exist in
general — a `Store` whose structural children loop is a perfectly good value —
and the paper had not noticed that `reconstruct`'s definedness is exactly what
`wf` (i) asserts rather than something the definition can assume. This is the
move the paper already made for printing (`def:print`) and evaluation. The
fueled `eval`/`recon` are separate and total; they are what `cor:fuel` is about.

**4. `Σ : Hash → Maybe Node`.** A function with decidable key equality, not an
association list or `Data.AVL` (the alternatives `agda/README.md` weighed).
Inclusion then reads "everything the smaller map says, the bigger map says
too", which is exactly how `thm:mono` and `thm:stability` are stated, and with
the store as a function the two halves of `thm:mono` (grows, never overwritten)
collapse into one. Finiteness is deferred: a `support` field joins the record
when M3's rewrite must enumerate `dom(Σ)`.

**5. `HashModel` is parameterized over the entry functor.** The record sketched
in `agda/README.md` was circular — `Hash : Set` and `hash : Node → Hash` as
sibling fields, where `Node` mentions `Hash`. `data Node (H : Set)` first, then
`HashModel (F : Set → Set)`, breaks the knot, and is also what makes the graph
layer reusable.

**6. The reuse boundary for arbor-stlc, decided.** `arbor-stlc.tex`
§"Mechanization note" deferred "parameterizing the store over an abstract node
signature" until the work began; it is `Arbor/NodeSig.agda` — entry type,
structural children, reference children, a map over both, and the laws relating
them. `wf`, callers, and the rewrite are written against it, so arbor-stlc's
two-sorted entry is a second instance rather than a fork. `Config` and the
transition relation stay concrete: arbor-stlc adds a `TypeOf` transition, so
genericity there would be false economy.

**Two findings about the paper, from doing this.**

- **`thm:mono` tacitly assumes the store is hash-keyed.** Its proof argues "each
  key is ⌈n⌉ for the very node n stored, so any collision on a key is with an
  identical node" — which needs to know that a stored entry sits at its own
  hash. No clause of `def:wf` says so. Isolated as `HashKeyed` and carried as a
  coherence clause; whether it belongs in `wf` is in `open-questions.md`.
- **`thm:stability`'s two `wf` premises are unnecessary.** The proof replays the
  derivation using only immutability of the entries it reads, i.e. `Σ ⊆ Σ′`
  alone. Both forms are in `Meta.agda`: the paper's, and the strengthening
  actually proved. A negative check confirms the remaining hypothesis is
  load-bearing (dropping `Σ ⊆ Σ′` breaks the proof).

**Also proved because the transcription needed them, and absent from the
paper:** `ingest-⇝` (reconstructing an ingested term returns it — leaned on
twice, by `Cache-sound`'s premise and by `lem:cascade-order`'s re-ingestion
step, without being stated) and `elab-func` (`def:elab` claims elaboration is a
partial *function*; that is now a lemma).

---

## 2026-08-05 — Milestone 2: coherence preservation, and four corrections to the paper

`arbor-core`'s well-formedness and safety milestone is proved. 15 of the paper's 17
statements are now discharged; what remains is constructing the migration rewrite ρ.
Proved in this pass: `thm:wf`, `thm:histcoh`, `cor:fuel`, `lem:closed-no-stuck`,
`prop:elab-print`, `cor:incremental`.

Mechanizing forced four amendments to the paper itself, listed here because they are
substrate-adjacent, not merely presentational. All are now fixed in
`paper/arbor-core.tex`.

**1. `lem:closed-no-stuck` was false, and is refuted.** Stated with only `wf(Σ)` and
`closed(t)`, the lemma does not hold: `closed(ref h)` is vacuously true for *every* h
(a reference carries no free variable), while `wf` clause (ii) constrains only the
references *of stored entries*. Nothing forces a stored hash to reconstruct to a closed
term — and nothing can, because storage is shallow and `dom(Σ)` is full of open subterm
nodes by design (`rem:nosort`). `agda/Arbor/Core/Counterexamples.agda` exhibits a
two-entry store `{⌈nvar 5⌉ ↦ nvar 5, ⌈nlam ⌈nvar 5⌉⌉ ↦ nlam ⌈nvar 5⌉}`, **proves**
`wf(Σ)` for it rather than assuming it, and derives `Σ ⊢ ref h ⇓ lam (var 5)` with the
value open. The repair is a premise that t's references denote closed terms — exactly
`Ingest`'s second premise and coherence clause (b) — and the conclusion must carry it
too, because the App case contracts against a value obtained from the function position
and needs *that* value's references.

**2. The `Eval` transition was missing `closed_Σ(h)`.** Consequence of (1): `thm:wf`'s
Eval case appeals to `lem:closed-no-stuck` for "the ingested value is closed", and the
repaired lemma needs h's target closed, which no other premise of the rule supplies. So
`def:transitions` gains it. Operationally free — p4 evaluates definitions, and a named
hash is closed by coherence (b) — but it is a real premise, not a formality: without it
coherence preservation is false.

**3. `wf` does not say the store is keyed by hash, and `thm:mono` needs it.** Recorded in
the M1 entry above; now carried as the `keyed` clause of configuration coherence, and
discharged for `ingest` and (as a spec field) for the rewrite.

**4. `prop:elab-print`'s side condition is redundant, and its proof sketch is not
executable.** The paper guards it with "every reference in t has a name outside Γ", which
an `Elab` derivation already witnesses at every `el-free` leaf. This matters more than a
tidy-up: `Name` is abstract in the substrate (`def:ns`: "an opaque string type"), so
there is no fresh-name supply, and the sketch's "choosing binder names away from the
finitely many aliases the subterm's references need" cannot be carried out at all. The
proof that does work is simpler than the sketch — printing returns the very surface term
the elaboration came from, since `Elab` and `Print` are the same relation read in
opposite directions.

**One design call inside the mechanization.** `thm:migosc` clause (c) is discharged from
a field of `RewriteData`, the record specifying what a migration rewrite must deliver,
rather than left open. Reason: `thm:wf` quantifies over *all* transitions, Migrate
included, so leaving migration's wf-preservation open would have blocked the entire
milestone. The obligation is not dissolved — it transfers to M3's construction of an
inhabitant, which must produce that field from ρ's definition, and `make status` reports
it as `[spec, M3]` rather than `[proved]` so the distinction stays visible.

**Where M3 is hard, from having looked.** ρ itself is definable: well-founded `Acc`
recursion on `wf` clause (iii) (already stated as well-foundedness of "is cited by"),
with the structural part handled by a dependent `substRefs` over the reconstruction. Two
real obstacles. First, the ρ-image has to be *registered*, which means folding over
`dom(Σ)` — and `Store` is a bare function with no finiteness witness. The fix is a
separate `Finite σ` record (support list + completeness) assumed only where the paper
needs it, rather than a change to `def:store` that would ripple through every proof.
Second, and harder: re-proving `wf` for the rewritten store. Its acyclicity clause needs
the paper's "referents are registered before referrers" argument, and ρ is **not
injective**, so acyclicity does not transfer along it — the registration order has to be
made explicit, which is the same machinery `lem:cascade-order` is about. Those two should
land together.

---

## 2026-08-07 — Merging the Unison read into the mechanization line, and one new corollary

`related-work`'s 11 commits merged into `formalism`. They branch from the same point (the
arbor-stlc landing) and are the only branch ahead of it that matters: `main`,
`docs/type-abstraction` and `p16-records` are strictly behind, and `p18-rich-editors`
carries the prototype line rather than the formalism.

The merge conflicted in exactly one place — both lines appended a new section to
`formalism/open-questions.md` — and both were kept: the Unison divergences sit with the
modeling questions, the mechanization questions lead into the Agda section.

**What the read changed in the proofs.** One new statement, `cor:transfer`, and it is
`thm:stability`'s proof verbatim. That is the finding rather than a caveat: stated only for
store *growth*, stability reads as a fact about one user's timeline, but nothing in the
argument reads the store's history — it replays a derivation over entries fixed by
immutability, and immutability does not care whose store the entries sit in. So the sharing
property a computational commons needs was already proved and merely stated too narrowly.
Unison relies on the same fact to sync test results across codebases. Proved as `cor-transfer`
(one line, `⇓-mono`); 16 of 18 statements now discharged.

Two further items are recorded in the paper rather than the proofs. `thm:alpha` now says what
it *withholds* (`rem:alpha-only`): being a biconditional on terms, it keeps `ref h` distinct
from what `h` reconstructs to, so inlining is not hash-preserving — the opposite of Unison,
whose reference node contributes the referent's hash to its parent. And `prop:print-elab` is
the property Unison's `update` depends on (render dependents to source, re-parse, re-typecheck)
but never states — it tests it over a regression corpus. That is the sharpest external
comparison the formalism has, and it is proved.

**What did not change, and why that was the risk worth checking.** Unison hashes a whole
strongly-connected component, so a definition is `(Hash, Pos)`. Had arbor followed, `Store`,
`wf`'s no-dangle clause, `callers_of`, ρ and `thm:alpha` would all have moved — a substantial
part of what is proved. It does not apply: `docs/design/03-content-addressing.md` works through
three closures and concludes that closing the cycle with a binder in the object language leaves
`Σ : Hash ⇀ Node` untouched and "no metatheory in this paper changes shape". So extrinsic
scoping, relations-for-partial-functions, the `NodeSig` boundary and the function-valued store
all stand.

**A cross-branch finding neither line had.** That design doc defers option B because "no
prototype has needed recursion" and says evaluating it "wants a prototype with `Fix` plus the
existing record machinery". **p19 is that prototype** — isorecursive `Mu` with explicit
`fold`/`unfold`, `Fix`, `Record` and `Variant` — and it reached the same conclusion
independently in a source comment ("you cannot hash a cycle without a fixpoint"). The two were
written a day apart on different branches. Recorded in `open-questions.md`; what to check before
promoting B from thread to choice is whether p19 exercised the *mutual* case or only the
self-referential one.

---

## 2026-08-07 — M3: the migration rewrite, constructed

`def:cascade`'s ρ is built rather than specified (`agda/Arbor/Core/Rewrite.agda`), and
`rewriteData` assembles a `RewriteData` inhabitant for any finite well-formed store. So
`thm:migosc` clause (c) is no longer a field projected out of an empty record: 17 of the
paper's 18 statements are discharged, and only `lem:cascade-order` remains — deferred, but
no longer a prerequisite for anything.

**Two findings, each of which overturned the plan recorded a day earlier.**

*The fold needs no dependency order.* The previous entry held that registration must
proceed referents-first, since registering an entry's image needs its rewritten references
closed — and that ρ's non-injectivity therefore made the paper's "referents are registered
before referrers" argument load-bearing. That is true of an **incremental** proof and false
of the construction. Every entry of Σ is in the support list, so every image is registered
somewhere in the fold, and ingest only ever adds; the reference targets are therefore closed
in Σ′ whatever order the fold ran in. What made this expressible is `ingest-tgt-big`, a
variant of the ingest lemma whose conclusion and reference premise are pinned to a fixed
larger store instead of the accumulator. No topological sort is needed anywhere.

*ρ's non-injectivity is real but not an obstacle.* Distinct entries can rewrite to one hash
— content-addressing working as intended, and the same phenomenon `thm:alpha` states
positively. It does defeat the obvious acyclicity argument, that accessibility transfers
along ρ pointwise. But the proof never has to choose a preimage: it follows an edge
**forward**, from an entry contributed by h's registration to a ρ̂-image of one of h's own
references, and recurses on Σ's accessibility of that reference. A colliding hash carries the
same node by (★), hence the same out-edges, so which preimage one arrived from is not
information the argument uses.

**Two transcription notes worth keeping.** ρ branches on `Dec (Recon σ h)` rather than on
a store lookup carrying its own proof: with-abstraction cannot abstract a term whose type
mentions the term being abstracted, so the first shape made every property of ρ unprovable.
And because ρ is defined by well-founded recursion, it is a function only up to `rho-irr`,
its independence from the accessibility proof — `Acc` is propositional only up to funext,
which `--safe` does not provide.

**Finiteness** is the one thing assumed rather than derived. Only registration uses it, so
it is a separate `Finite` record — a change to `def:store` would have rippled through every
proof in the development for the sake of one fold. Whether the paper should say explicitly
which results need finiteness is in `open-questions.md`.
