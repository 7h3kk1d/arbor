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
