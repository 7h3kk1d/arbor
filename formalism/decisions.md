# Formalism decisions

Running log of modeling commitments for the arbor formalism. Lightweight ADR: each entry has a
date, the decision, a rationale, and noted alternatives. A reversal gets a new dated entry; old
entries are not edited. This log concerns how we formalize. Substrate design commitments live in
`docs/design/decisions.md`.

---

## 2026-07-23 — First formal artifact: `arbor-core` over untyped λ

The formalism begins with one LaTeX document, `arbor-core`, over the untyped λ-calculus (the p4
fragment). It covers the term store, the namespace, a derived evaluation cache, and an edit
calculus. Successors climb the same TAPL ladder the prototypes follow, with types next.

**Rationale.** This applies the prototype method (`docs/design/decisions.md` 2026-04-21,
"disposable experiments") to proof artifacts: use the smallest language that still exhibits the
core payoff of content addressing. Untyped λ suffices to state and prove no-silent-breakage and
evaluation stability. Types can be added later.

**Alternatives.** Start at STLC (p6). Rejected because types obscure the two headline theorems,
which concern identity and naming, not typing. Formalize the whole p17 stack at once. Rejected on
the same step-in-slowly grounds the prototypes use.

---

## 2026-07-23 — Abstract hash + indirect `Ref` (generalizing p4)

`Hash` is an opaque type with an injective `hash : Node → Hash`, a stated axiom in LaTeX and a
single postulate in Agda. Stored terms reach callees through a **`Ref(hash)` node** resolved
against the store. This reintroduces the `Ref` constructor that p4 elided: p4 inlines closed
subterms at resolve time (`resolver.re`, `canonicalize.re`) and has no `Ref`.

**Rationale.** Without indirection the interesting metatheory is vacuous. The callers relation,
explicit migration and immutability of callers under edits all need cross-definition edges, and
inlining leaves none to break or rewrite. `docs/design/03` mandates `Ref(hash)` ("every
cross-definition reference … is a hash reference — `Ref(hash)` — never a name"). p4 is the
restricted single-checkout instance where inlining suffices. The formalism models the substrate
ideal and cites p4 as the special case.

**Injectivity as idealization.** Cryptographic collision resistance is modeled as exact
injectivity (`docs/design/03`: "collisions are treated as impossible in practice"). The concrete
BLAKE2B + tag-byte encoding (`node.re`) is one realization of the abstract `hash`, not part of
the theory.

**Alternatives.** `Hash := canonical term` (interning) makes injectivity hold by `refl` with no
postulates. Rejected for v1: it collapses `Ref` back to inlining, leaves the store semantically
inert, and weakens the theorems we want. Recorded as the postulate-free fallback in
`agda/README.md`.

---

## 2026-07-23 — Scope: migration without mints; well-formedness gate

`arbor-core` includes the full edit calculus: `bind`/`rebind`/`unbind`, append-only binding
history with `name(vN)` orphan rendering, the `callers_of`/`callers_closure` reverse DAG, atomic
`multi_rebind`, and pin/follow/explicit as points in the `scope × user-in-loop` space
(`docs/design/04` §"Update strategies"). It defers the mint/thread identity axis (p10/p11) and
all types.

**Consequence: the gate.** p11 gates its cascade on a real typecheck (`update_strategy.re` calls
`Typecheck.check_top`; p11 is typed via the p9 line). Untyped, this becomes a
**well-formedness gate**: closed, and all refs resolve. Substituting one closed `Ref` for another
preserves both, so the gate is near-total and `follow` almost always succeeds. The paper presents
this as a feature. It shows that the interesting failure modes of `follow` are the ones types
introduce, which motivates the sequel.

**Consequence: lineage without mints.** p11 uses the mint to say "these hashes are versions of one
thing" independently of any name. `arbor-core` recovers lineage more weakly, from the per-name
binding history `H` (the sequence of hashes a name has held). Mint-as-identity is a documented
successor.

**Rationale.** The user asked for updates and explicit migration in the first artifact. Starting
small argues against a second identity axis. History-based lineage suffices to state migration
correctness and orphan rendering.

**Alternatives.** Minimal (store + naming, migration deferred): rejected, since the user named
migration. Full p11 with mints and the `follow-clean` oracle: deferred to a successor.

---

## 2026-07-23 — Evaluation cache is a first-class derived aspect

The evaluation cache `E : Hash ⇀ Value` is modeled as a **derived** aspect, immutable once
written, beside the **asserted** namespace `N` in the Attachment layer. It answers the design
goal that already-evaluated terms are never re-evaluated.

**Rationale.** The Attachment layer then appears with its two real instances, names and eval
results (`docs/design/02`, p4 `attachment.re`). It lets us prove the dual of no-silent-breakage:
cache soundness under store growth, and migration incrementality (only new hashes are evaluated).
It also explains p4's rule that `StepLimit` is never cached (`eval.re`): only defined evaluations
are stable, so only they are cacheable.

**Alternatives.** Leave the cache out and mention re-evaluation informally. Rejected: "don't
re-evaluate" is one of the two theorems that justify the design.

---

## 2026-07-23 — Evaluation is a relation; the step budget is an implementation

Evaluation is a partial relation `Σ ⊢ t ⇓ v` (CBV, weak-head normal form, undefined on
divergence). p4's `default_step_limit = 10000` budget and its `Value | Stuck | StepLimit` result
(`eval.re`) implement this partial function. `Value` and `Stuck` are the defined cases;
`StepLimit` means "not yet known to be defined."

**Rationale.** A relation is the right object for metatheory (stability, determinism), and it
makes T5/T6 clean: cacheable ⟺ defined. The Agda bridge is a fuel-indexed
`eval : ℕ → Σ → Term → Result` with a fuel-monotonicity lemma (`eval_k = Value v ⟹ eval_{k+1} =
Value v`). That lemma is why `StepLimit` must not be cached and `Value`/`Stuck` may.

**Alternatives.** Formalize the budget directly. Rejected: budget-relative results are not stable
as the budget grows, so caching them by hash alone is unsound. The relation states this precisely
without a magic constant in the theory.

---

## 2026-07-23 — Naming modeled at the substrate primitive (exact resolution)

Resolution is the substrate primitive `N : Name ⇀ Hash`, with exact lookup and two outcomes
(Found / NotFound). `Name` is an opaque string. The substrate imposes no hierarchy and no
ambiguity (`docs/design/04:22-33` lists suffix resolution as a non-goal).

**Honest divergence from the prototypes.** p9/p11 implement, at the editing layer, a
`resolve_query` with dotted-segment longest-suffix matching and a distinct `Ambiguous` error
(`namespace.re:283-334`), alongside the exact `resolve` (`namespace.re:260`). The formalism takes
the exact `resolve` as the substrate primitive. Suffix resolution is an editing-layer elaboration
definable over it, deferred to a successor. This follows `04`'s split ("names are an
editing-layer concern") and keeps the first artifact's resolution judgment two-valued.

One prototype guard is not lifted: `bind`/`rebind` reject reserved keyword names
(`namespace.re:29-57`). That is a surface-grammar UX guard; substrate names are opaque strings.

**Alternatives.** Model `resolve_query` + `Ambiguous` as substrate resolution. Rejected: it is
editing-layer per `04`, and three-valued resolution complicates the elaboration round-trip without
touching the headline theorems.

---

## 2026-07-30 — Definition roots, guarded transitions, ref-acyclicity (first review pass)

A design review of the draft, before any successor artifact, found three interlocking gaps, now
fixed in the paper. All three trace to one cause: dropping p11's `Named` wrapper sort left
"definition" informal, and several statements leaned on it.

**1. Rooted store.** A store is now a pair `(Σ, R)` with `R ⊆ dom(Σ)` the **definition roots**.
Before, `wf` was not a predicate on `Σ`: its closedness clause said "bound in `N`", which crosses
into the namespace. `callers` and the cascade quantified over all of `dom(Σ)`, which under shallow
storage includes every anonymous open subterm node (`Var 1` under a `Lam` is an entry).
`Ingest`/`Migrate` populate `R`, and `wf`, `callers` and the cascade are parameterized by it. p11
realizes `R` as the `Named_term`/`Named_type` sort minus the mint; p4 leaves it implicit.

**2. Guarded transitions + configuration coherence.** `Ingest` gains premises `closed(t)` and
`refs(t) ⊆ R`. It previously had none, so ingesting `ref h₀` at a dangling `h₀` falsified
wf-preservation as stated. The old proof sketch appealed to "edit-time resolution", an
editing-layer fact. That fact is now a lemma (elaboration produces closed terms with
`refs ⊆ ran(N)`) feeding real premises. `Bind`/`Rebind` gain `h ∈ R`. The namespace layer alone
cannot check this: p11's `namespace.re` accepts any hash, and validation happens one level up, in
`multi_rebind`'s `target_exists` and the editor's resolve step. A config-level transition can
state it directly. `Rebind` stays total in the name, like p11's `rebind`, which also binds fresh
names. New **coherence** invariant on configurations: `wf(Σ,R)`, `ran(N) ⊆ R`, history
`Some`-hashes ⊆ `R`, history coherence, and `Cache-sound`. Its preservation is now the T4
statement.

**3. Ref-acyclicity as a `wf` clause.** Injectivity (★) alone permits hash fixpoints
(`hash(Ref h) = h`), and the old `wf` did not exclude reference cycles. `reconstruct` stops at
`Ref` leaves, so only structural cycles were ruled out. A cyclic reference graph makes `callers*`
cyclic, so migration's "dependency order", used but never defined, need not exist. New `wf`
clause (iv): the reference graph is acyclic. Dependency order is now defined as a topological sort
of the in-scope callers closure, which exists by (iv), and a new lemma proves the cascade result
order-independent. The lemma matters because p11's `callers_closure` returns DFS discovery order,
not dependency order. That is harmless in p11, whose references are deep structural containment
substituted against a body-keyed mapping. It is load-bearing here, where substitution rewrites
shallow `Ref` leaves.

**Alternatives.** (a) Derive the root set from the configuration (`ran(N)` ∪ ref-targets ∪
history hashes) instead of carrying `R`. Rejected: `Ingest` would lose its meaning, since
"register a definition" is what adds a root, and `wf`'s parameterization becomes awkward to
mechanize. (b) Keep `wf` weaker and prove acyclicity only as an invariant of reachable
configurations. Rejected: `wf` should be self-contained (a store either supports migration or
does not), and preservation is provable anyway from `Ingest`'s new premises.

---

## 2026-07-30 — No definition sort: names confer definition-hood; migration is a whole-store rewrite

Supersedes the previous entry's rooted-store commitment (same day), after working the design
through concrete cascade examples. The guarded-transitions and ref-acyclicity commitments stand,
reworked to fit.

**The stance.** A mint-free artifact has no unit of intent. A name is a label used to match
λ-terms in the store, and hash equality is the only identity. Hence:

- (a) The store carries **no definition sort**. An explicit root set `R` is provenance identity,
  a degenerate mint. p11's `Named` wrapper, cited as precedent in the previous entry, carries a
  mint for this reason.
- (b) **Naming a closed stored term is what treats it as defined.** `Bind`/`Rebind` require only
  `closed_Σ(h)`, so any closed stored hash may be named, including one that only ever existed as
  a subterm.
- (c) **Migration propagates structurally.** Updating `Z → Z'` rewrites every store entry
  reaching `Z`, scope-guarded at reference crossings and name bindings, and names follow their
  hashes. Unit-scoped following returns with the mint successor.

**What changed in the paper.** `wf(Σ)` is again a predicate on the store alone: (i) reconstruct
defined, (ii) reference targets closed (the E-Ref soundness condition; not circular, since a
target's closedness ignores its own refs), (iii) reference graph acyclic. Config is back to
`⟨Σ, N, E, H⟩`; coherence requires name and history hashes to be closed. The cascade is replaced
by a **whole-store rewrite `ρ`**, defined by recursion on the combined structural+reference graph.
That graph is acyclic by (i)+(iii), since mixed cycles project to reference cycles. `ρ` is
order-free by construction, so the dependency-order machinery survives only as the
sequential-pass lemma bridging to p11's implementation. Strategies become propagation scopes
(`Pin ↦ ∅`, `Follow ↦ dom(Σ)`, `Explicit(S) ↦ S`) consulted at reference crossings and name
bindings. Faithfulness improves twice. Under Follow, aliases of the edited definition now rebind,
as in p11 via its wrapper-as-caller path; the pre-review `U` missed them. Under Pin only the
edited name moves, as in p11.

**Findings.**
- The untyped gate is total, not near-total. Rewriting preserves closedness and registers
  referents before referrers, so `Follow` cannot fail and migration atomicity is vacuous here. The
  contract is stated anyway because the typed successor's gate needs it.
- Two alternatives were tried and rejected. **Bare closedness as the definition sort** makes every
  `Ref` stub and anonymous closed fragment a "definition", filling callers/cascade bookkeeping with
  junk pairs. **"Ever named" (`ran(N)` ∪ history) as cascade scope** makes Follow under-propagate
  through orphaned intermediates: the rewritten orphan is never named, so a second migration pins
  at it. Lineage through unnamed intermediates is mint-shaped state. The whole-store rewrite has
  neither problem. Its cost is that "what was migrated" is characterized structurally
  (`ρ(h) ≠ h`), not as a list of user-facing units, which do not exist at this rung.

---

## 2026-07-30 — arbor-stlc: content-addressed types, commit-and-report migration, the clean oracle

Second artifact: `paper/arbor-stlc.tex`, the STLC rung, written as a delta over arbor-core.
arbor-core is unchanged. Cross-references use `xr-hyper` with a `core:` prefix, and every external
reference renders as "arbor-core Definition N.M".

**Content-addressed types** (user decision), following p9's extension and diverging from p6, which
stores annotations inline in `Lam` nodes. The store becomes two-sorted (term entries + type
entries `TBool`/`TArrow(h,h)`), `Lam` carries a type hash, and structural type equality reduces to
hash comparison. Sub-decision: type entries are shallow where p9's are deep
(`Definition.Type(Ty.t)` stores whole types). Shallow is the store's uniform discipline, and the
two are equivalent up to interning since types are finite and ref-free. Sort discipline folds into
reconstruct-definedness, with no new wf clause. Rejected: p6-style inline annotations, which make
types second-class and block the hash-equality/TypeOf story.

**T-Ref is new formal content.** p6 has no reference rule. It inlines referenced bodies at
resolution, so no reference reaches `infer`. The formalism types references through the store
(`Σ;∅ ⊢ reconstruct(Σ,h) : T ⟹ Σ;Γ ⊢ ref h : T`), the same idealization as arbor-core's
reintroduction of `Ref`. The judgment does not read the Θ aspect, as ⇓ never reads E.

**Store and namespace are fully permissive; typing is observational.** No transition gains a
typing premise. Ill-typed closed terms may be stored and named, a legitimate mid-refactor state.
The substrate computes and reports typing through `typeof`, the hash-valued Θ aspect (p9-style;
p6's inline `Type_of(Ty.t)` recorded as a divergence), the standing `broken(Σ,N)` query and the
migration residual, but never gates on it. p6's resolver typecheck is editing-layer policy above
these observations. (User direction: breakage is visible and queryable, never forbidden.)

**Migration commits and reports, diverging from p11's abort.** Migrate commits as in arbor-core.
The typed rung adds a **report**: `residual` is the set of followers (at name level, rebound names)
that were well-typed before and are not after, the "continue the refactor" list. p11's
`Cascade_typecheck_failed` abort is recoverable as editing-layer policy ("if clean then migrate
else ask"). Headline theorems:
- **Residual exactness**: broken′ = (broken ∖ healed) ∪ residualN. A migration breaks exactly what
  it reports.
- **Type-preserving migration is total**: same `typeof` at the seed ⟹ empty residual under every
  scope, with no rechecking. This promotes p11's `same_type` short-circuit from a dead code path
  to a theorem.
- **Local soundness**: a well-typed term evaluates safely regardless of ill-typed neighbors,
  because its T-Ref closure carries its own support. p6's "Stuck is theoretically unreachable…
  defence in depth" becomes the progress theorem.

**Finding: `clean` is not a stable aspect.** clean/residual are decidable before migrating, and the
oracle and the report are definitionally aligned. p11's `dry_run` only approximates this: it checks
`Named_term` callers only, ignores the strategy filter, and does not accumulate the substitution map
its own cascade builds. But clean is a property of the current store, and ingesting a new caller
can falsify it. p11 caches `follow-clean:v1` as a write-once bool keyed `BLAKE2B(h_old‖h_new)`, a
key that encodes nothing that grows with the store. A cached `true` therefore goes stale and is
never corrected, which is unsound. The one soundly cacheable fact nearby is the type-preservation
test, since typing of a stored hash is absolute. Sound keys for the general predicate are in
open-questions.

**Deferred:** type names in N and type aliasing, type-level migration (p11's `Named_type` path),
p6's second language and translation, mints, Agda mechanization of this rung.

---

## 2026-07-30 — Hash-valued cache; single identifier leaf; printing as a judgment

Three fixes that close the remaining faithfulness items from the review (5a, 5b) and formalize the
surface↔store round-trip.

**The cache is store-to-store.** `E : Hash ⇀ Hash` maps a term's hash to the hash of its value,
which is itself ingested into the store. Key and value are both store terms, so the cache never
touches surface syntax, as the design requires. This matches p4: its cached aspect is
`Eval_value(Hash.t)`, and its evaluator ingests every result and intermediate reduct
(`reconstruct → beta → ingest`). The `Eval` transition now grows `Σ` (monotonicity updated), and
`Cache-sound` reads through `reconstruct`. The `⊢ ⇓` relation stays term-level and pure; the
headline theorems still rely on evaluation being pure in `(Σ, h)`. The fueled hash-level `eval`
that ingests is its implementation (Agda bridge updated). Rejected: term-valued `E`. It is
equivalent for the theorems but parks deep terms outside the store, against the convention that
derived aspects are hash-valued (p5 `Translation_target`, p9 `Type_of`).

**Single identifier leaf.** The draft's two-leaf surface (`sVar` bound, `sName` free) was
unfaithful. p4's `surface_ast.re` has one `Var(string)`, and a parser cannot tell which is which.
Its `El-Bound` was also ambiguous under shadowing (`Γ(i) = x` admits any matching index). Now there
is one leaf. `El-Bound` takes the least index (innermost binder wins) and `El-Free` requires
`x ∉ Γ`. The rules are disjoint and elaboration is a partial function.

**Printing is a judgment, and both round-trips are stated.** Printing `Δ;N ⊢ t ⇚ s` is relational.
`P-Lam` picks any binder name and `P-Ref` any alias, and the leaf side conditions (least index;
`x ∉ Δ`) reject capturing choices. `pretty.re` is one deterministic refinement (fresh-name supply,
alphabetically first alias). Round-trip A: print-then-elaborate recovers the stored term exactly,
for every choice the printer could make. Round-trip B: elaborate-then-print holds up to `≈`, the
kernel of elaboration, which is generated by capture-avoiding binder renaming and alias swap. This
closes the "Round-trip precision" open question. Orphan forms `x(vN)` stay display-only, not
surface syntax. Printing is undefined at a hash with no name and no history, which the editing
layer can always avoid.

---

## 2026-08-05 — Mechanization: statements as types, extrinsic scoping, relations for partial functions

First Agda code (`agda/`). Milestone 1 is proved; every other `arbor-core` statement is stated
and marked open. Six modeling commitments follow, three of which reverse or correct what
`agda/README.md` had recorded.

**Why now.** The Agda side had drifted from the papers. There was no code against ~37 stated
theorems, `agda/README.md` sat untouched through the arbor-stlc landing, arbor-stlc's
mechanization note deferred a reuse-boundary decision that was never made, and both the README and
this log indexed theorems as `T1`–`T8`, a numbering neither paper uses. Paper `\label`s are now the
only names, and `scripts/check-labels.sh` enforces it.

**1. Statements are types, not postulates.** Agda's `--safe` rejects postulates. Each numbered
result is a named type in `Arbor/Core/Meta.agda` (`thm:nsb` ↦ `Thm-nsb`), proved when that type
has an inhabitant. The library assumes nothing: (★) is a field of the `HashModel` record it is
parameterized over, and `Arbor/Core/Model.agda` exhibits an inhabitant, so no proved statement is
vacuous. A later proof cannot lean on an open obligation, because there is no term to lean on.
Rejected: postulating open statements in a non-`--safe` module. That would let M2 build on
unproved lemmas but weakens what a green build means.

**2. Extrinsic scoping, reversing the recorded lean.** `agda/README.md` and `open-questions.md`
both leaned intrinsic (`Term : ℕ → Set`, closedness as a type). That collides with shallow
storage. `dom(Σ)` contains every subterm ever ingested, open ones included (`rem:nosort`), so
`reconstruct` at `⌈nvar 5⌉` yields an open term. An indexed `Term` would force its codomain to
package a level existential, and a non-canonical one, since `var 5` inhabits `Term 6`, `Term 7`, ….
Extrinsic also matches the paper: `def:closed` is already an inductive relation, and the `Ingest`
premise already passes closedness explicitly. α-equivalence stays definitional either way, because
it comes from de Bruijn and not from the indexing, so `thm:alpha`'s (⇐) half is still `cong`.
Accepted cost: `beta`-preserves-closedness becomes an explicit M2 obligation instead of a typing
fact.

**3. The paper's partial functions become inductive relations.** `reconstruct` is `σ ⊢ h ⇝ t` with
a functionality lemma, and the migration rewrite ρ will be the same. As Agda functions they would
need termination arguments that do not exist in general: a `Store` whose structural children loop
is a valid value. The paper had not noticed that `reconstruct`'s definedness is what `wf` (i)
asserts, not something the definition can assume. The paper already made this move for printing
(`def:print`) and evaluation. The fueled `eval`/`recon` are separate and total, and they are what
`cor:fuel` is about.

**4. `Σ : Hash → Maybe Node`.** A function with decidable key equality, not an association list or
`Data.AVL` (the alternatives `agda/README.md` weighed). Inclusion then reads "everything the
smaller map says, the bigger map says too", which is how `thm:mono` and `thm:stability` are stated.
With the store as a function, the two halves of `thm:mono` (grows, never overwritten) collapse into
one. Finiteness is deferred: a `support` field joins the record when M3's rewrite must enumerate
`dom(Σ)`.

**5. `HashModel` is parameterized over the entry functor.** The record sketched in
`agda/README.md` was circular: `Hash : Set` and `hash : Node → Hash` were sibling fields, and
`Node` mentions `Hash`. Declaring `data Node (H : Set)` first and then `HashModel (F : Set → Set)`
breaks the knot and makes the graph layer reusable.

**6. The reuse boundary for arbor-stlc, decided.** `arbor-stlc.tex` §"Mechanization note" deferred
"parameterizing the store over an abstract node signature" until work began. It is
`Arbor/NodeSig.agda`: entry type, structural children, reference children, a map over both, and the
laws relating them. `wf`, callers and the rewrite are written against it, so arbor-stlc's
two-sorted entry is a second instance, not a fork. `Config` and the transition relation stay
concrete, because arbor-stlc adds a `TypeOf` transition and genericity there would be false
economy.

**Two findings about the paper.**

- **`thm:mono` tacitly assumes the store is hash-keyed.** Its proof argues "each key is ⌈n⌉ for the
  very node n stored, so any collision on a key is with an identical node". That needs a stored
  entry to sit at its own hash, and no clause of `def:wf` says so. Isolated as `HashKeyed` and
  carried as a coherence clause; whether it belongs in `wf` is in `open-questions.md`.
- **`thm:stability`'s two `wf` premises are unnecessary.** The proof replays the derivation using
  only immutability of the entries it reads, i.e. `Σ ⊆ Σ′` alone. `Meta.agda` has both the paper's
  form and the strengthening actually proved. A negative check confirms the remaining hypothesis is
  needed: dropping `Σ ⊆ Σ′` breaks the proof.

**Also proved, because the transcription needed them; absent from the paper:** `ingest-⇝`
(reconstructing an ingested term returns it), used without statement by `Cache-sound`'s premise and
by `lem:cascade-order`'s re-ingestion step; and `elab-func`, since `def:elab` claims elaboration is a
partial function and that is now a lemma.

---

## 2026-08-05 — Milestone 2: coherence preservation, and four corrections to the paper

`arbor-core`'s well-formedness and safety milestone is proved. 15 of the paper's 17 statements are
discharged; constructing the migration rewrite ρ remains. Proved in this pass: `thm:wf`,
`thm:histcoh`, `cor:fuel`, `lem:closed-no-stuck`, `prop:elab-print`, `cor:incremental`.

Mechanizing forced four amendments to the paper. They are listed here because they are
substrate-adjacent, not presentational. All are fixed in `paper/arbor-core.tex`.

**1. `lem:closed-no-stuck` was false, and is refuted.** Stated with only `wf(Σ)` and `closed(t)`,
it does not hold. `closed(ref h)` is vacuously true for every h, since a reference has no free
variable, while `wf` clause (ii) constrains only the references of stored entries. Nothing forces a
stored hash to reconstruct to a closed term, and nothing can: storage is shallow and `dom(Σ)` is
full of open subterm nodes by design (`rem:nosort`). `agda/Arbor/Core/Counterexamples.agda`
exhibits a two-entry store `{⌈nvar 5⌉ ↦ nvar 5, ⌈nlam ⌈nvar 5⌉⌉ ↦ nlam ⌈nvar 5⌉}`, proves `wf(Σ)`
for it, and derives `Σ ⊢ ref h ⇓ lam (var 5)` with an open value. The repair is a premise that t's
references denote closed terms, which is `Ingest`'s second premise and coherence clause (b). The
conclusion must carry it too, because the App case contracts against a value obtained from the
function position and needs that value's references.

**2. The `Eval` transition was missing `closed_Σ(h)`.** This follows from (1). `thm:wf`'s Eval case
appeals to `lem:closed-no-stuck` for "the ingested value is closed", and the repaired lemma needs
h's target closed, which no other premise supplies. `def:transitions` gains it. It costs nothing
operationally, since p4 evaluates definitions and a named hash is closed by coherence (b). It is
still a real premise: without it coherence preservation is false.

**3. `wf` does not say the store is keyed by hash, and `thm:mono` needs it.** Recorded in the M1
entry above. Now carried as the `keyed` clause of configuration coherence, and discharged for
`ingest` and, as a spec field, for the rewrite.

**4. `prop:elab-print`'s side condition is redundant, and its proof sketch is not executable.** The
paper guards it with "every reference in t has a name outside Γ", which an `Elab` derivation
already witnesses at every `el-free` leaf. More importantly, `Name` is abstract in the substrate
(`def:ns`: "an opaque string type"), so there is no fresh-name supply, and the sketch's "choosing
binder names away from the finitely many aliases the subterm's references need" cannot be carried
out. The proof that works is simpler: printing returns the surface term the elaboration came from,
since `Elab` and `Print` are one relation read in opposite directions.

**One design call inside the mechanization.** `thm:migosc` clause (c) is discharged from a field of
`RewriteData`, the record specifying what a migration rewrite must deliver, instead of being left
open. `thm:wf` quantifies over all transitions, Migrate included, so leaving migration's
wf-preservation open would have blocked the milestone. The obligation transfers to M3's
construction of an inhabitant, which must produce that field from ρ's definition. `make status`
reports it as `[spec, M3]`, not `[proved]`, so the distinction stays visible.

**Where M3 is hard.** ρ is definable by well-founded `Acc` recursion on `wf` clause (iii), already
stated as well-foundedness of "is cited by", with the structural part handled by a dependent
`substRefs` over the reconstruction. Two obstacles remain. First, the ρ-image must be registered,
which means folding over `dom(Σ)`, and `Store` is a bare function with no finiteness witness. The
fix is a separate `Finite σ` record (support list + completeness), assumed only where the paper
needs it; changing `def:store` would ripple through every proof. Second, and harder, `wf` must be
re-proved for the rewritten store. Its acyclicity clause needs the paper's "referents are
registered before referrers" argument, and ρ is not injective, so acyclicity does not transfer
along it. The registration order has to be made explicit, which is the machinery
`lem:cascade-order` concerns. The two should land together.

---

## 2026-08-07 — Merging the Unison read into the mechanization line, and one new corollary

`related-work`'s 11 commits were merged into `formalism`. Both branch from the arbor-stlc landing,
and `related-work` is the only branch ahead of it that matters: `main`, `docs/type-abstraction` and
`p16-records` are strictly behind, and `p18-rich-editors` carries the prototype line, not the
formalism.

The merge conflicted in one place: both lines appended a section to `formalism/open-questions.md`.
Both were kept. The Unison divergences sit with the modeling questions; the mechanization questions
lead into the Agda section.

**What the read changed in the proofs.** One new statement, `cor:transfer`, whose proof is
`thm:stability`'s verbatim. Stated only for store growth, stability reads as a fact about one
user's timeline. But the argument never reads the store's history. It replays a derivation over
entries fixed by immutability, and immutability does not depend on whose store holds the entries.
The sharing property a computational commons needs was already proved and only stated too narrowly.
Unison relies on the same fact to sync test results across codebases. Proved as `cor-transfer` (one
line, `⇓-mono`); 16 of 18 statements are now discharged.

Two further items are recorded in the paper, not the proofs. `thm:alpha` now says what it withholds
(`rem:alpha-only`): as a biconditional on terms, it keeps `ref h` distinct from what `h`
reconstructs to, so inlining is not hash-preserving. Unison is the opposite: its reference node
contributes the referent's hash to its parent. And `prop:print-elab` is the property Unison's
`update` depends on (render dependents to source, re-parse, re-typecheck) but never states; Unison
tests it over a regression corpus. That is the formalism's sharpest external comparison, and it is
proved.

**What did not change.** Unison hashes a whole strongly connected component, so a definition is
`(Hash, Pos)`. Had arbor followed, `Store`, `wf`'s no-dangle clause, `callers_of`, ρ and
`thm:alpha` would all have moved, a large part of what is proved. It does not apply.
`docs/design/03-content-addressing.md` works through three closures and concludes that closing the
cycle with a binder in the object language leaves `Σ : Hash ⇀ Node` untouched and "no metatheory in
this paper changes shape". Extrinsic scoping, relations for partial functions, the `NodeSig`
boundary and the function-valued store all stand.

**A cross-branch finding neither line had.** That design doc defers option B because "no prototype
has needed recursion" and says evaluating it "wants a prototype with `Fix` plus the existing record
machinery". p19 is that prototype: isorecursive `Mu` with explicit `fold`/`unfold`, `Fix`, `Record`
and `Variant`. It reached the same conclusion independently in a source comment ("you cannot hash a
cycle without a fixpoint"). The two were written a day apart on different branches. Recorded in
`open-questions.md`. Before promoting B from thread to choice, check whether p19 exercised the
mutual case or only the self-referential one.

---

## 2026-08-07 — M3: the migration rewrite, constructed

`def:cascade`'s ρ is now built, not specified (`agda/Arbor/Core/Rewrite.agda`), and `rewriteData`
assembles a `RewriteData` inhabitant for any finite well-formed store. `thm:migosc` clause (c) is no
longer a field projected from an empty record. 17 of the paper's 18 statements are discharged. Only
`lem:cascade-order` remains; it is deferred and no longer a prerequisite for anything.

**Two findings, each overturning the plan recorded a day earlier.**

*The fold needs no dependency order.* The previous entry held that registration must go
referents-first, since registering an entry's image needs its rewritten references closed, and that
ρ's non-injectivity therefore made "referents are registered before referrers" load-bearing. That is
true of an incremental proof and false of the construction. Every entry of Σ is in the support list,
so every image is registered somewhere in the fold, and ingest only adds. The reference targets are
therefore closed in Σ′ whatever order the fold ran in. `ingest-tgt-big` makes this expressible: a
variant of the ingest lemma whose conclusion and reference premise are pinned to a fixed larger
store instead of the accumulator. No topological sort is needed.

*ρ's non-injectivity is real but not an obstacle.* Distinct entries can rewrite to one hash. That is
content addressing working as intended, the same phenomenon `thm:alpha` states positively. It
defeats the obvious acyclicity argument, that accessibility transfers along ρ pointwise. But the
proof never chooses a preimage. It follows an edge forward, from an entry contributed by h's
registration to a ρ̂-image of one of h's own references, and recurses on Σ's accessibility of that
reference. A colliding hash carries the same node by (★), hence the same out-edges, so the argument
never uses which preimage it came from.

**Two transcription notes.** ρ branches on `Dec (Recon σ h)`, not on a store lookup carrying its own
proof. With-abstraction cannot abstract a term whose type mentions the term being abstracted, so the
first shape made every property of ρ unprovable. And because ρ is defined by well-founded recursion,
it is a function only up to `rho-irr`, its independence from the accessibility proof. `Acc` is
propositional only up to funext, which `--safe` does not provide.

**Finiteness** is the one thing assumed, not derived. Only registration uses it, so it is a separate
`Finite` record; changing `def:store` would have rippled through every proof for one fold. Whether
the paper should say which results need finiteness is in `open-questions.md`.

---

## 2026-08-07 — Retracting the reuse-boundary decision

The 2026-08-05 entry recorded, as commitment 6, that the arbor-stlc reuse boundary "is decided now"
and is `agda/Arbor/NodeSig.agda`. That was premature and is retracted.

The record exists and `Arbor/Core/Node.agda` instantiates it, but nothing consumes it. This was
found by grepping for uses while planning M4, after three documents had asserted it:
`decisions.md`, `agda/README.md`, and `paper/arbor-stlc.tex` §"Mechanization note". All three are
corrected.

It went unused because every graph-layer lemma reaches an entry through the reconstruction judgment
`σ ⊢ h ⇝ t`, whose right-hand side is a deep term. The proofs are shaped by the term language, not
the node signature, so `NodeSig` abstracts the wrong thing. An abstraction over `⇝` itself, with
`⇝-func`, `⇝-mono` and `⇝-∈dom` as fields, would be consumed by `acyclic-transfer`, `⊑-edges`,
`update-dom` and callers.

A design decision recorded before any code consumed it drifted from the code, as the papers had
drifted from the mechanization. The label-check script catches paper-vs-Agda drift. It does not
catch a decisions-log claim about the Agda's internal structure. Whether that warrants its own check,
such as dead-code detection over the library, is open.

M4's shape is therefore open again; the options and their costs are in `open-questions.md`.

---

## 2026-08-07 — Bringing the papers back in line with the mechanization

Both `\section{Mechanization}` sections had fallen behind the Agda, and three findings from the
typed rung belonged in the body, not only in this log. Corrected now, since the label-check
discipline exists so the papers do not describe an earlier state of the proofs.

**What was stale.** `arbor-core.tex` claimed sixteen of eighteen statements and said "what remains
is the construction of ρ itself". ρ has since been constructed, so `thm:migosc` is projected from a
record with a witness. `arbor-stlc.tex` said mechanization of that rung "has not begun" and left the
reuse boundary as two open options. It has begun, and the answer was neither option.

**What the papers now say.**

- *arbor-core §sec:mech* gains a fifth transcription choice: `Term` as the fixpoint of a signature,
  with the two arities per shape that the shallow/deep split forces. It states that nothing in the
  document changes: `def:core`'s four constructors are recovered as pattern synonyms, so a proof
  still case-splits on Var/Lam/App/Ref as written. That fact made the refactor acceptable, and it
  belongs in the paper.
- *arbor-core `rem:noorder`*, after `lem:cascade-order`: the construction needs no dependency order,
  and `def:deporder`/`lem:cascade-order` serve only as a bridge to p11's implementation. It gives the
  reasons both expectations were wrong: well-formedness need not be re-established after every
  registration, and ρ's non-injectivity does not block acyclicity because the argument follows edges
  forward and never picks a preimage.
- *arbor-stlc `prop:ty-ident`*: in the mechanization it is not "the typed analogue of thm:alpha" but
  the same theorem, since types and terms are shapes of one signature.
- *arbor-stlc `lem:ty-stable`*: its two `wf` premises are unused, for the same reason
  `thm:stability`'s are.
- *arbor-stlc `rem:subst-empty`*: `lem:subst`'s general context is unused, since `thm:pres` is its
  only consumer and is stated at ∅. The generality is not free: for closed `v` the incremental
  shifting is the identity and the TAPL 9.3.8 apparatus collapses.

**A recurring kind of correction.** Several are the same finding: a statement carries a hypothesis
or generality that nothing consumes. `thm:stability`'s `wf` premises, `prop:elab-print`'s side
condition, `lem:ty-stable`'s `wf` premises, `lem:subst`'s general context. Mechanizing finds these
reliably because an unused premise shows up as an unused argument. Watch for this in the next rung.
The cost is more than inelegance: `lem:subst`'s generality would have committed the proof to
machinery the document does not need.

---

## 2026-08-25 — `mini-unison`: modeling another system, and the choices that took

`paper/mini-unison.tex` formalizes Unison's content addressing and update scheme. It is a
**contrast artifact, not a rung**: it models another system, faithful to `db60ce2`, and proposes
nothing. It exists because Unison has no publication, so every comparative claim in `docs/` was
being made against prose.

**Choices.**

1. **Simple types, not an abstract acceptance gate.** Unison's `update` re-typechecks, and the
   interesting half of `update` is the failure branch: evict the broken definitions and hand them
   back as source. Untyped, that branch is unreachable. An abstract `Ok : File → Bool` parameter was
   rejected because it carries no content of its own, and the object language is small enough that
   real types cost little.
2. **Named ABTs, de Bruijn only at hash time.** The opposite of `arbor-core`, and forced: `update`
   renders stored definitions back to source a human must read, so the author's binder names must
   survive storage. `arbor-core` states its round trip only up to name choice (`prop:elab-print`)
   because it does not keep them.
3. **Ingest takes a binding group, not a term.** This is how `hashComponents` works, it is where
   mutual recursion lives, and it avoids inventing a `letrec` former Unison does not have.
4. **Disjoint notation** (`macros-unison.tex`, everything `\U`-prefixed). Overloading `\Store` would
   hide that the two stores have different types, which is the document's first substantive claim.
5. **Paper-only.** Outside the Agda discipline, so a green `make check` is never read as saying
   anything about this document. Recorded in `README.md`.

**Three findings, two of which corrected the source read.**

- **`thm:ordering`: presentation-independence of component hashing is conditional.** When two cycle
  members are structurally equivalent modulo intra-cycle references, the stable sort falls back to
  input list order, which is always `Map.toList`, so the component's hash depends on the members'
  names. The condition is decidable (`cor:detect`), and the implementation aborts instead of storing
  a name-dependent hash. `doHashCycle` returns a warning and says "hashing always succeeds even if it
  generates warnings"; `crashOnHashingWarning` at the caller turns it into an exception.
  `docs/systems/unison/02-store-and-hashing.md` had recorded this as "canonical form for an SCC is a
  *partial* function" and "Unison detects this and refuses". Both corrected.
- **`prop:transparent`: transparency holds only at `Pos = 0`.** `ReferenceDerived h 0` contributes
  literally `h`; `ReferenceDerived h i` for `i ≠ 0` hashes opaquely, tagged with the position. So
  transparency and the component model are not independent features. The same file had stated
  transparency "at any position". Corrected.
- **`rem:twoorders`: component hashing computes two orderings.** The permutation order fixes the de
  Bruijn indices used while hashing member bodies. A second order, sorted by a hash of those indices,
  fixes the `Pos` indices in stored references. Nothing observable depends on the difference, but a
  reimplementation that conflated them would produce different hashes, and no prose description
  mentions it.

**Method.** All three were invisible to a careful prose read of the same source three weeks earlier,
by the process that produced `docs/systems/unison/`. Formalizing another system's design is a way of
reading it that finds different things than prose reading. That argues for formalizing the next
system too, not only surveying it.

---

## 2026-08-25 (later) — `mini-unison`: two more corrections, and where the notes go

**A fourth finding, `rem:unhash`.** The user asked whether "references are replaced by their hashes"
was true. It was not, and checking turned up something new. An intra-component reference has three
representations:

| | form |
| --- | --- |
| while hashing | a **variable**, resolved to an index into the cycle frame |
| in memory, after `hashComponents` | `Derived(h, i)` with the component's own fresh hash |
| serialized | **no hash** — `Reference' Text (Maybe Hash)`, `Nothing` marking self |

The bytes on disk are not the bytes that were hashed, and re-deriving a stored component's hash
requires undoing the substitution first. `unhashComponent` is that inverse, documented as "replaces
all `Nothing` self-referential hashes with a variable reference to the relevant piece of the
component", and the hash-validation path calls it before re-hashing. The encoding is sound: writing a
component's own hash into its members' serialized bytes would need the bytes before the hash, the
circularity components exist to break. But "the hash of a definition is the hash of what is stored"
is false for cyclic definitions in three distinct ways.

**The prose error behind it.** The introduction said a definition is identified by the hash of its
syntax "after binder names have been erased and references to other definitions have been replaced by
*their* hashes". This was wrong twice. A reference is not replaced: it already carries the referent's
hash, and `hash1` computes the hash of that node. At position 0 the result equals the referent's hash,
which is `prop:transparent`, a consequence and not the mechanism. And the sentence implied
intra-cycle references are hashes, which they cannot be. There is a substitution at ingest, but it
replaces free variables naming other members of the same file with reference terms: names become
references, not references hashes.

**Where notes go, decided.** The document should be readable outside this project, including by
Unison's authors, so it carries no process narration: no account of what an earlier draft got wrong,
which prose read missed what, or which claim corrected which file in `docs/`. That material lives
here. The paper states findings positively and this log carries the history. A header comment in
`paper/mini-unison.tex` says so.

One piece of method stays in the paper, stated without autobiography: several observations concern
behaviour the code fixes precisely but no prose description states, and writing a system down
precisely enough to prove things about it differs from reading it. A reader can evaluate that claim.
"We read this three weeks ago and missed it" reads as a confession, not a result.

---

## 2026-08-25 (third) — `mini-unison`: presentation order, and where references enter

Two reader-facing problems, both raised by the user.

**References were presented as if you write them.** `def:uterm` had `\uref{r}` as a term former with
no indication that no source text contains one. The parser settled it. `resolveHashQualified` maps an
unqualified name to `Term.var`, so a plain name parses to a variable; only a hash-qualified name
resolves at parse time. Variables become references later, in `bindNames`, and only for names
denoting things already in the codebase. Names denoting other bindings in the same file stay
variables because they are shadowed. Unresolvable or ambiguous names are left free on purpose ("leave
free for telling user about expected type", "ambiguous: leave free for TDNR").

That gives a six-stage table, now `rem:staging` in §2, which organizes the whole document: written
name → variable → reference-or-variable → variable again inside a cycle during hashing →
`Derived(h,i)` in memory → no hash when serialized. The last two rows are `rem:unhash`, which used to
arrive unannounced in §3.

**Ordering fixes.**

- `def:uterm` used `\Reference` two sections before its definition. `def:uref` now opens §2, with a
  forward pointer to `def:ustore` for why the derived arm is a pair. The reader needs the shape to
  read the grammar, not yet the reason.
- `def:ureferent` ("names denote referents, not references") sat in §3 among storage definitions. It
  is about naming and now opens §4, which also gained a lead-in.
- `rem:uref-delta` (why typing a reference is a lookup) forward-referenced `def:typeinhash`. It now
  follows it in §3.2.
- "Summary of results" moved from §1.8, after the scoping boilerplate, to §1.3. The order is now what
  this is → why → what was found → caveats → how to read.
- §3 was "The store, components, and the hash". With references moved out, it is "The store and the
  hash".

Reference transparency stays before component hashing. It is a clause of the term hash function and
belongs next to it, so the progression is term → reference → component → guarantees. Its discussion
mentions cycle heads, which needs positions, so it carries a forward pointer to §3.4.

---

## 2026-08-25 (fourth) — `mini-unison`: worked examples, and a claim they falsified

Added §7, six worked examples: ingesting one definition, a bare alias, a recursive group, the
ambiguous-ordering case, an update that succeeds, and an update that fails.

**They falsified a claim in the paper.** `rem:transparent` carried both consequences the source
comment gives for reference transparency: that `x = 1 + 1` and `y = x` "hash the same", and that
inlining is hash-preserving. Working the alias example through `def:comphash` shows neither follows.

A reference at position 0 contributes the referent's component hash. Inlining the body would
contribute the referent's term hash. `hashComponent` puts two accumulations between them, even at
*k* = 1: `commonTokens = Tag 1 : map Hashed hashes`, then `overallHash = acc (map Hashed hashes')`.
So `⌈X⌉ = ⌈Y⌉` would require `⌈Nat.+ 1 1⌉ = #x`, where `#x` is built from `⌈X⌉`. The equation asks
the hash function for a fixed point of a particular shape.

The comment's own caveat is this condition: "so long as `Reference.Derived` ctors are created using
the same hashing function as is used here". One construction satisfies it:
`hashClosedTerm tm = ReferenceId (ABT.hash tm) 0` builds a reference from a term's own hash.
References made that way are interchangeable with their referents; ones made by `hashComponents` are
not.

The term-level property (`prop:transparent`) is right and the definition-level reading is not.
`rem:transparent` now states only the former and points at `rem:transparency-limit`, which works out
the difference. The appendix lists it as the claim we would most like checked, since it bears on a
frequently repeated description of the system.

**Method.** This is the document's fourth finding, the second from a user question, and the first to
contradict something we had already written down and believed. Prose can restate a source comment
without testing it against the definitions; a worked example cannot.

## 2026-09-02 — `mini-unison`: a citation audit, and five things it corrected

Every `\src{}` tag in `paper/mini-unison.tex` was resolved against the Unison source at the pinned
commit `db60ce2`. The local checkout is still at that commit, so nothing had drifted. Fourteen file
keys, roughly forty cited ranges. Every claim resolved to code; nothing was unlocatable. The findings
fall into three groups.

**Confirmed, including the claim we were least sure of.** `rem:transparency-limit` is right. The
construction that satisfies the strong reading of reference transparency, `hashClosedTerm`, which
builds `Derived(⌈t⌉, 0)` from a bare term hash, exists and is used only for anonymous and evaluated
expressions, never for storing a named definition. Component hashing puts two `accumulate` layers
between a member's term hash and its component hash even at *k* = 1, so `x = 1 + 1` and `y = x` do
not hash the same. The appendix now states this as a claim we would like checked, not as a reading we
might have got wrong. Its two remaining entries are questions about intent (is the `Pos = 0` boundary
deliberate; are the two orderings meant to be distinct), which the source cannot answer.

Also confirmed: the three-step component hash and its two orderings, the
`IncompleteElementOrderingError` detect-and-abort path, position-0 transparency, type-in-hash
including the replaces-an-existing-annotation clause, the SHA3-512 token stream, the namespace
relation and `BranchV3`'s removal of it, the absence of any invalidation logic, the PPE asymmetry,
the "world's weirdest implementation of AST substitution" comment, and the round-trip corpus.

**Five claims were wrong.**

1. `def:watch` had the cache as `Hash ⇀ Hash`, "both store hashes". It is `(Hash × Pos) ⇀ Term`: the
   key is a reference, addressed as a definition is, and the value is the result term stored inline
   as a blob. Caching a result does not put it in the store. `thm:wstable` and `cor:wnoinval` survive
   untouched; the soundness argument never depended on the codomain.
2. `def:update` step 5 said the offending definitions' names are removed. The implementation removes
   every name the file binds and every dependent it rendered, whatever the typechecker said about
   each. Added `rem:step5`.
3. `ex:update-fail` therefore had the wrong end state. It showed `scale` rebound to a new hash. Nothing
   is ingested on the failure path, and `scale` is among the names the file binds, so after a failed
   update the namespace contains neither `scale` nor `double`. What is delivered is `F ∪ S`, the
   user's file plus the rendered dependents, not `S`.
4. **`ex:ambiguous` asserted something the definitions do not give.** It said renaming `ping` to
   `zing` would change `#c`. It does not. The last step of `def:comphash` accumulates the `nh` values
   sorted, so a rename that only permutes that multiset leaves the component hash fixed and moves only
   the position assignment. `thm:ordering`(b) is now split: (b1) which member is referenced
   transparently always becomes name-dependent; (b2) the hash itself does in general but not for every
   tie. The example carries a witness for each.
5. `prop:follow-only` recommended `add` as the abstention gesture. `add` has been an alias for
   `update` since 2025; there is no separate command. The substance survives (a fresh name has no
   dependents), but the sentence described a command that no longer exists.

**Nine more were imprecise** and are now hedged or re-cited:
- "annotations are excluded" collided with the type ascription that is hashed and carries
  `def:typeinhash`;
- `rem:version`'s hash indirection is provisioned but never exercised; every write hardcodes
  version 2;
- `def:unames` cited a name-lookup table deleted in 2025;
- `def:unconf`'s derived view is all-or-nothing, not a filter, which is why `def:uscope`'s `S` is a
  precondition and not an intersection;
- the watch GC touches the wrong table and runs only in a schema migration;
- the `Same | Subtype | Different` datatype outlived the machinery that consumed it;
- `rem:unhash` quoted one module and cited another;
- `def:comphash`'s "name order" is really `(freshening-id, var-kind)` order, which matters on the
  re-hash path and now has a footnote explaining why the round trip works anyway.

Nine citation ranges were off by a few lines. Eleven `\src{}` tags had been stripped by the
2026-08-25 "fewer citations" pass without closing up the punctuation, leaving text like "keep
resolving ."; they are restored where the claim is load-bearing.

**Method.** Fifth finding entry, and the first from a targeted audit. The three methods fail
differently. Reading produces the model. Worked examples falsify claims the model makes about itself
(2026-08-25, fourth). An audit falsifies claims about the source. Errors 2, 3 and 5 are of the last
kind: behaviour described from a plausible reading of code we had looked at. Error 4 is neither. It is
arithmetic about our own `def:comphash` that neither reading nor auditing catches, and a worked
example got it wrong because the example was written to illustrate a theorem, not to test it. The
prototype now being built settles it by computation, a fourth method and the only one that would have
caught it.

## 2026-09-02 (later) — Watches, and a proposition the model was missing

A question about the prototype, *"watch expressions don't get added to the store?"*, exposed a gap in
the paper and a bug in the code.

**The gap.** `def:watch` and `rem:watchkey` described the cache and observed, from a schema comment,
that its key "need not be a definition". Nothing in the model said where such a key comes from.
`def:bindgroup` had a file as a map from names to terms, with no watch expressions, so a reader could
not derive the situation the remark described. The remark reported a fact about Unison's SQLite
schema instead of stating a property of the model.

**What was added.** `def:ufile`: a file is a binding group plus a sequence of anonymous watch
expressions, closed over the group's names. `def:uingest` now hashes both in one pass (Unison's
`allTerms = join tlcs ++ join (snd <$> watches)`, under a comment reading "includes watches") and says
what differs afterwards. A definition's component is stored and its name bound. A watch's is neither;
only its value is kept, at the reference ingest just computed. Hence **`prop:watchdom`**: some
reachable configurations have `dom(W) ⊄ dom(Σ)`. The proof turns on unreachability: a watch is
unnamed, `tdeps` ranges over named references, and `render` has no name to print it by, so its hash
never enters Σ.

`rem:watchdom-ok` states the payoff. `thm:wstable`'s proof never used that the key is in the store,
only that everything the key reads is. A cache entry denoting nothing is therefore outside the
argument's hypotheses, not an edge case it must exclude. Had the cache been specified as a map on
definitions, `prop:watchdom` would have been a bug report. This is the technical content of "there is
no *is a definition* status attached to stored data", which the document had only asserted.

**Both halves are checked.** In `p21-unison-model`, `test_watchkey` ingests a buffer with two watches
and asserts they are hashed, absent from the store, unnamed and cached. In real Unison, a codebase
with one evaluated watch has the watch's hash interned in `hash` and no `hash_object` row for it: an
identity with no object behind it, `prop:watchdom` in a live system.

**Also measured, since it was cheap and settles a claim `cor:wnoinval` only argues for.** One
expensive watch saved once: 5.79 s. The same watch saved twice: 5.82 s. Two different watches of
equal cost: 11.45 s. Re-saving does not re-evaluate, and the control rules out a timing artifact.
Across separate `ucm` processes against a warm codebase: **0.084 s**. The cache is consulted across
saves and across process boundaries, reading `watch_result` from SQLite.

*(Corrected later the same day. The cross-process figure was first measured as 5.85 s and reported as
inconclusive, on the theory that `--save-codebase-to` had failed to round-trip the namespace. Both
were wrong. `-S` round-trips correctly: the saved codebase has its branch head, its cursor and 748
terms. Plain `ucm transcript` ignores `-c/--codebase` and always builds a fresh codebase
(`Main.hs:507`); only `transcript.fork` reads an existing one. The "warm" run was cold. A null result
from a tool whose flags you have not read is not a null result.)*

**Method.** Sixth finding entry, the second from a user question (after the fourth), and the first
found by someone using the prototype instead of writing it. The omission was invisible to the test
suite because `test_watchkey` had been written against the `Watch` module's shape, constructing a hash
by hand and putting it in the cache, not against the pipeline. A test that fabricates its own input
cannot notice that nothing produces that input.

## 2026-09-03 — Save is not commit, and the paper had them fused

A request about the prototype, *"I want typing in the scratch buffer to do the same as saving does in
Unison. Unison separates commit from saving"*, named a conflation in the model as well as a missing
feature in the code.

**What the paper said.** `def:uingest` folded three things into one step: computing references,
extending `Σ`, and (after the previous entry's watch work) extending `W`. `def:update` step 4 read
*"if it typechecks, ingest and rebind"*. Together they say identity is conferred by committing. That
is wrong, and it made `prop:watchdom`'s proof reach for `update` unnecessarily.

**What the implementation does.** `loadUnisonFile` → `evalUnisonFile` is the save path, reached from
the file-watch event. It typechecks, hashes the whole file including watches, evaluates the watches
against a code lookup covering the file itself, and writes each result with `putWatch`, `only when
(not isHit && cache)`. It contains no `addDefsToCodebase` and no `putTerm`. A
`TypecheckedUnisonFile` carries a reference for every member in `hashTermsId`, and nothing has entered
the codebase. A save confers identity and populates the cache; a commit stores and names.

**What was added.** `def:usave`, a transition on `⟨Σ, N, W⟩` that fixes `Σ` and `N` and extends only
`W`. `def:uingest` is now purely the hashing function and extends nothing. `def:update` step 4 says it
stores at a hash `def:usave` already computed. `prop:watchdom`'s proof is rebased on save and is
stronger for it: no commit is required or could help, since `update` adds components for the file's
definitions and the witnessing file has none. Also added: `rem:saveident` (identity comes from
typechecking; the gate is a gate on identity, not storage) and `rem:watchsplit` (both kinds are hashed
alike and diverge only at commit).

**This sharpens the commit-gate finding.** The gate is still real and still the largest divergence
from arbor. But it bites earlier than recorded: a buffer that does not typecheck has no identities at
all, not merely nothing stored. And the caching `cor:wnoinval` describes is reachable with no commit,
which is why re-saving an expensive watch is free.

**Both halves are checked.** `p21-unison-model` gained `Scratch.save` (hash without storing, evaluate
watches, write on miss only) alongside `Scratch.analyze` (inert). `Ingest.hash_components` gained
`~register` as the only difference between the two paths, with a test asserting save and commit agree
on the hash. Three more tests: a save confers references while storing and naming nothing; re-saving
does not re-evaluate, and reverting an edit hits an entry nothing invalidated; analysis alone writes no
cache entry. 33 tests, 32/32 labels mirrored.

**Method.** Seventh finding entry, third from a user question, and the second in two days from someone
using the prototype. The previous entry found watches missing; this one found the step they belong to
missing too. Both were invisible to a test suite that exercised `Update.run` and never modelled the
gesture before it. With no `save` in the code, nothing objected to the paper fusing `save` into
`ingest`.

---

## 2026-09-03 — mini-unison gains a merge section, and the no-conflict property is conditional

**Decision.** Model `merge` in `mini-unison.tex` as §7, a function of three namespaces over one store.
State the property "if you didn't edit a definition it can't conflict" in the only form in which it
is true: conditional on names being stable. Fourteen new statements (`prop:merge-store` …
`prop:merge-print-parse`) and three new worked examples.

**Why the conditional form.** The natural statement, *a definition neither party authored is never
conflicted*, is false, and the counterexample is ordinary. Merge classifies each name's change by
comparing a **syntactic hash**: the definition's hash recomputed with each reference replaced by the
name the merge gives it (`def:synhash`). Propagation is invisible to that hash, because an updated
dependency keeps its name and both versions render the same (`lem:prop-silent`). That is what makes
automatic propagation compatible with three-way merge. But it needs `ν(r) = ν(r′)`, and a rename
breaks it. Renaming a dependency while editing it makes every dependent look hand-written.

So `thm:noconflict` is stated in the classifier's own vocabulary (a conflict needs two syntactic
changes), and `cor:noedit` carries the side condition explicitly. `rem:cor-weight` points at the
hypothesis, because the corollary reads like the unconditional property until you look at it.

**A structural parallel.** The paper now has two conditional guarantees with computable side
conditions and opposite handling. `thm:ordering` fails when two cycle members are indistinguishable;
that is decided at ingest and the implementation aborts (`cor:detect`). `cor:noedit` fails when a name
moved; that is computed (`Merge/Rename.hs` builds the gained/lost name partition needed) and at
`db60ce2` is consumed only by a debug printer (`cor:unchecked`). Merge proceeds either way and reports
a conflict it cannot distinguish from a real one. Worse, rename detection groups by syntactic hash, so
it finds renames that carry no edit, which do not conflict, and cannot find the case that does.

`rem:libcanon` is the exception. `canonicalizeNamesForSynhashing` strips a mangled semver off
`lib.base_1_0_0`, forcing old and new library names to be equal, expressly "to make it more likely
that we classify things that are *probably* true propagated updates as such." That restores the side
condition by hand for the one renaming scheme whose shape is known in advance.

**Three failure modes.** Running the same ancestor three ways against `ucm` 1.1.1 gave three different
wrong answers from the same cause:

| what the other branch does | outcome |
|---|---|
| renames a dependency **and** edits it | spurious conflict; the reported side is the ancestor's own code (`ex:merge-rename`) |
| renames a dependency, no edit | **no conflict**, merge still fails — the file renders a name the merged namespace deleted (`ex:merge-dangling`) |
| edits a dependency the other branch renamed | **clean merge**, rename silently undone, stale alias left behind (`ex:merge-dup`) |

The third is the worst and was least expected. `(delete, update)` is not a conflict; the delete is
discarded ("perhaps only temporarily, because it's easier to implement"); and a rename is a delete plus
an add. The add survives, the delete does not, and where the ancestor had one definition the result
has two.

The second yielded `prop:merge-asym`: merge is not commutative. The rendering environment prefers the
target branch's names while the merged namespace applies both branches' renames, so the same two
branches merge cleanly in one direction and fail in the other. Verified by running one pair both ways.

**What this does to `prop:print-parse`.** It generalizes it. Update's round trip stays within one
namespace. Merge renders under one naming environment and parses under another
(`prop:merge-print-parse`), and nothing requires the names it emits to exist in the second. That is
the mechanism of `ex:merge-dangling`.

**Also recorded.** `prop:merge-store`: two independently grown stores union without reconciliation,
the formal content of "content-addressed codebases have no merge conflicts". It is true, and
`rem:merge-store` says it is close to vacuous: nothing anyone calls a conflict lives in Σ.
`rem:syn-only`: `Eq (Synhashed a)` compares only the syntactic hash, so once two definitions render
differently no content hash is consulted. A content-addressed system decides its merges by name.

**Restated over user gestures.** `cor:noedit`'s hypothesis is about `ν`, which no user thinks about,
so the section restates it in terms of gestures. `def:gestures` (new, in §4) gives
`move`/`alias`/`delete` as the three operations that touch only `N`. `rem:renamefree` says these are
free everywhere in §§3–6: one row, no caller broken, no cache invalidated, nothing rewritten because no
definition contains a name. That is the usual argument for content addressing, and it is true.
`def:namestable` defines *authored on a branch* and *name-stable*: no move, alias or delete, and no
update that binds a name to a definition that already has one. The last clause is needed because by
`thm:ualpha` writing out an existing definition produces an alias unasked. `cor:noedit-user`: **if both
branches are name-stable, no name authored on neither branch is conflicted.**

Contrapositively (`rem:onlyrenames`): *the only user action that can put someone in front of a
conflict in a definition they never opened is a name moving.* Adding, updating and deleting are safe at
any depth. Three details of the hypothesis: it binds both branches, not only the one that abstained;
an `alias` suffices without a `move`, because the PPE picks the fewest-segment name
(`PrettyPrintEnv/Names.hs:141-151`); and unconflicted is not unseen, since `def:umerge` step 2 pulls
dependents of conflicts into the same file.

**Evidence, and a drift.** §7 rests on different evidence from the rest of the paper: the three
examples were observed against `ucm`, not computed by the model, and `sec:paths` says so.
`check-labels.sh` now reports 49 statements, 17 not mirrored: the merge section plus the three
user-level statements. p21 has not caught up, and `p21-unison-model/open-questions.md` records what
implementing it would take. `def:gestures` is cheap and could land alone: three functions on the
namespace relation, no hashing.

**Method.** Eighth finding entry, and the first correcting a property the user proposed instead of one
an earlier draft asserted. The proposed property is the right one to want; the algorithm does not have
it.

---

## 2026-09-03 — the presentation-independence guarantee is a property of a call site, not of the hash

**Decision.** Correct `rem:notpartial`, retitle `cor:detect` ("is checked" → "is computed"), and add
`rem:innercycle`. The claim that Unison "never silently produces a name-dependent identity" is true of
top-level components and false one nesting level down.

**What was wrong.** The paper and `docs/systems/unison/02-store-and-hashing.md` both said internal
`let rec` bindings ignore the ambiguity warning *"because there the ordering is not observable."* That
gloss was ours. The source says only *"we don't want to error on that when hashing internal
let-bindings"* (`ABT.hs:171-172`), and the ordering is observable, in the enclosing definition's hash.

**Witness**, `ucm` 1.1.1, three definitions of the same function:

| | hash | |
|---|---|---|
| `m1 n = (a x = b x; b x = a x; a n)` | `#2d5bs110vr` | |
| `m2 n = (p x = q x; q x = p x; p n)` | `#2d5bs110vr` | members renamed — no change |
| `m3 n = (b x = a x; a x = b x; a n)` | `#qqn1r2c1od` | members reordered — different definition |

`m1 = m2` is `thm:ualpha` as advertised. `m1 ≠ m3` is new, and the pair rules out the listing being by
name. Lifting the same `a`/`b` bindings to the top level aborts with `E253299`.

**Mechanism**, already half-stated in `def:comphash`: step 2's `sortOn` is stable, so tied members keep
their listing order, that order fixes their de Bruijn indices, and the indices are inside the enclosing
hash. The footnote about `Map.toList` and `Var.reset` covers the top-level and re-hash paths. The inner
path is a third listing order it did not consider.

**Why this gets its own entry.** The failure is along a different axis. `thm:ordering`'s failure is
name-dependence; this one is source-order dependence, and `m1` and `m3` are indistinguishable to a
reader in a way `foo`/`bar` are not. It also generalizes. `hashComponent` is total and warns, and
whether the warning becomes an abort is a policy each caller decides. Unison has two callers and they
decided oppositely. Any substrate that hashes recursive groups inherits that shape: side condition in
the substrate, abort at the caller, and the callers must agree.

**Method.** Ninth finding entry. The evidence had sat in a scratch transcript since 2026-09-02,
recorded only as an aside about let-binder names. Writing §7, which needed `thm:ualpha` to be exactly
right, exposed the contradiction with a sentence already in the paper. The model reproduces the
top-level abort but cannot reach the inner path: p21's object language has no `let`, so
`hash_component` has one caller and the two-caller divergence cannot arise. Recorded in
`p21-unison-model/open-questions.md` as out of reach.

---

## 2026-09-03 — merge, built; and two things §7 had wrong

**Decision.** `p21-unison-model` implements §7 (`src/synhash.re`, `src/merge.re`). 49 statements, 49
mirrored, 45 tests. Building it corrected the paper twice and settled one open question. This is the
fifth time the prototype has corrected the paper, which is why it exists.

**Correction 1: `def:mergediff` read as if propagation is discarded.** It said `prop` is "separated out
and set aside", and the first implementation dropped those names entirely. Every clean merge then lost
half its result. A definition rebuilt by `update` on one branch is still that branch's version.
Withholding it from the diff, so nobody is asked about it (`cor:noedit`), is different from dropping it
from the namespace. `def:umerge` step 1 now applies the propagated bindings first and the resolved
operations on top.

**Correction 2: `def:umerge` step 2 was too narrow.** It said `D` is the conflicted definitions and
their transitive dependents. That misses the no-conflict case: a definition neither branch touched can
still point at a version of a dependency the merge moved, and if it is not rebuilt the merged namespace
keeps a stale reference. Upstream computes this downwards from a core dependency set whose second
source is easy to miss: the other party's changed names resolved on this branch, "no matter what its
hash is". The model characterizes the same set upwards. A referent is **superseded** when the ancestor
bound a name to it and the merged namespace no longer does, and `D` is every name whose definition
reaches a superseded referent. An update supersedes, a rename supersedes, and a definition whose
dependencies still mean what they meant is left alone. `ex:merge-dangling` is a no-conflict merge that
must still rebuild something, so the paper's own example did not typecheck against its own step 2.

**`rem:merge-soundness`, settled enough to state.** The question was whether the silent resolution in
the second row of `def:mergecombine` (equal syntactic hashes, different referents, Alice wins, no
report) can hide a real disagreement. It cannot, because the collapse cannot go all the way down. Equal
synhashes with different referents means some dependency differs, and at that dependency's name the
question recurs. `def:synhash` records literals, structure and types verbatim and falls back to the
content hash for an unnamed referent, so a difference invisible at every level does not exist. The
witness is a test: both branches update a shared dependency, `show` resolves silently, and the conflict
lands on `fmt`. That is the right division of labour, since `show` is not what the parties disagree
about. Stated as an argument, not a theorem, and the remark says so.

**Method.** Tenth finding entry. The label check passed before the tests did: 49/49 mirrored with a
`diff` that dropped propagated updates. A mirror check verifies that the prototype mentions every
statement, not that it implements any. This is the first time that gap has been visible. The tests
caught both corrections.

---

## 2026-09-04 — an alias breaks it too, and the pinning claim was wrong

**Decision.** Correct `prop:follow-only` and `rem:onlyrenames`, add `ex:merge-alias`, and broaden the
contrapositive from renaming to changing which names a definition has. Prompted by the question whether
adding a name could break merge the way moving one does, which `rem:onlyrenames` already claimed in a
bullet, from reading `prioritize`, without checking.

**Checked: the outcome depends on timing.**

*An alias before the update pins the dependents.* Two `ucm` runs differ in one `alias.term`. Without it
the dependent is rebuilt (`#7eac2fu0hj`). With it the dependent is unchanged (`#601qnj4906`) and now
renders as `show n = f n`. `update` selects and rebuilds it, and the rebuild is the identity: rebuilding
goes through text, the renderer shows the dependency under a name the old definition still has, and
parsing resolves back to where it started.

That is a counterexample to `prop:follow-only` as written: *"there is no way to request that some
callers be left pointing at the old definition. The nearest thing to that is to choose a name nothing
depends on."* There is a nearer thing. Pinning cannot be requested but can be arranged, through the
naming layer instead of an option. That uses `prop:print-parse`'s round trip as a control surface, and
it is the only per-caller propagation control the system has.

*An alias after the update conflicts*, as a rename does. `ν` picks the new alias for the new version
while the superseded one keeps its original name from the other branch, so an untouched dependent
renders differently and is filed as authored. This is the mechanism the old bullet asserted, and it is
right, but only in the after case, which the bullet did not say.

**What was wrong in the prose.** `rem:onlyrenames` was titled "only renaming does this" and said the
only user action that can hand someone a conflict in untouched code is *a name moving*, then carried a
bullet saying an alias suffices. It contradicted itself and `def:namestable`, which correctly excludes
`move`, `alias` and `delete`. Fixed in the remark, its title and the summary. Nothing is removed in the
alias case: every ancestor name stays bound and `fmt` still means what its owner means by it. Adding a
name was enough.

**Mirrored.** `ex:merge-alias` in the paper and `test_merge_alias` in p21 cover both timings. The model
reproduces each, since `Ppe.name_of` and `Synhash.name_of` both take the alphabetically first name and
so tie-break as `prioritize` does for single-segment names. 46 tests.

**Method.** Eleventh finding entry, and the second where the defect was a claim I had reasoned from
source and not run. The first was the inner-`let rec` gloss. Both concerned what a pretty-printer
picks.
