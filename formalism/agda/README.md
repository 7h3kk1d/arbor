# arbor-core — Agda mechanization

Machine-checked transcription of `paper/arbor-core.tex`. **Milestones 1 and 2 are
proved — 16 of the paper's 18 statements.** What remains is the construction of
the migration rewrite ρ (M3). Every statement is *stated* in Agda whether or not
it is proved, so drift between the paper and the proofs is visible rather than
silent.

```sh
make check    # agda --safe Everything.agda
make status   # what is proved, what is open, per paper label
make labels   # paper labels with no counterpart in the Agda sources
```

Toolchain: **Agda 2.7.0**, **agda-stdlib 2.1**. The stdlib is currently resolved
through the machine-local `~/.agda/libraries`, which points at
`~/Projects/plfa/standard-library`; `arbor-core.agda-lib` declares
`depend: standard-library` and does not care where it lives.

## What `--safe` buys, and what it does not

`make check` passes under `--safe` with **no postulates, no `TERMINATING`
pragmas, and no unsolved metas**. The paper's single axiom (★, `def:hash`) is a
*field* of the `HashModel` record that the development is parameterized over,
and `Arbor.Core.Model` exhibits an inhabitant of that record — so nothing is
assumed and no proved statement is vacuous.

What a green build does **not** mean is that the paper is proved. Unproved
statements appear in `Arbor/Core/Meta.agda` as named types with no inhabitant.
That is deliberate: a type with no term cannot be leaned on by a later proof,
whereas a postulate can. `make status` is the honest progress metric.

    16 of 18 statements discharged.

## Status

`Arbor/Core/Meta.agda` mirrors the paper: each statement is a type whose name is
the paper's label (`thm:nsb` ↦ `Thm-nsb`), proved when that type has an
inhabitant (`thm-nsb`). Run `make status` for the live table.

| Paper | Agda | Status |
| --- | --- | --- |
| `thm:alpha` — hashing collapses exactly α | `thm-alpha` | proved |
| `thm:nsb` — no silent breakage | `thm-nsb`, `thm-nsb-eval` | proved |
| `thm:mono` — monotonicity and immutability | `thm-mono` | proved |
| `thm:stability` — evaluation stability | `thm-stability` | proved |
| `thm:wf` — coherence preservation | `thm-wf` | proved |
| `thm:histcoh` — history coherence | `thm-histcoh` | proved |
| `cor:cache` — evaluate at most once | `cor-cache` | proved |
| `cor:fuel` — only divergence is non-cacheable | `cor-fuel` | proved |
| `cor:incremental` — migration is incremental | `cor-incremental` | proved |
| `cor:transfer` — stability under transfer | `cor-transfer` | proved |
| `lem:determinism` | `lem-determinism` | proved |
| `lem:closed-no-stuck` | `lem-closed-no-stuck` | proved (corrected) |
| `lem:elab-premises` | `lem-elab-premises` | proved |
| `prop:print-elab` | `prop-print-elab` | proved |
| `prop:elab-print` | `prop-elab-print` | proved (corrected) |
| `prop:namefree` | — | by construction |
| `thm:migosc` — atomicity and scope | `thm-migosc` | from spec, M3 |
| `lem:cascade-order` | — | deferred, M3 |

`thm:migosc` is discharged from `RewriteData`, the record specifying what a
migration rewrite must deliver. That is not a proof dressed up as one: the
obligation transfers wholesale to M3's construction of an inhabitant. It is a
field rather than an open statement because `thm:wf` quantifies over *all*
transitions, Migrate included, and would otherwise have been unprovable until
M3 — the design call recorded in `../decisions.md`.

Beyond the paper, these are proved because the transcription needed them:
`⇝-func` and `⇝-mono` (reconstruction is functional and survives store
growth — the workhorse of `thm:stability`), `elab-func` (`def:elab`'s claim
that elaboration is a partial *function*), `ingest-⇝` (reconstructing an
ingested term returns it — leaned on twice by the paper without being stated),
the scoping lemmas around `beta` that extrinsic scoping makes explicit, and
`acyclic-transfer` plus the `ingest-*` family in `Preservation.agda`, which are
what `thm:wf`'s four-sentence proof sketch actually expands to.

## What mechanizing found

Four gaps in the paper, all now fixed there and cross-referenced from here.

1. **`lem:closed-no-stuck` was false as stated.** `Arbor/Core/Counterexamples.agda`
   refutes it — with `WF σ` *proved*, not assumed, for an explicit two-entry
   store. `closed (ref h)` holds for every `h`, while `wf` constrains only the
   references of stored entries, so a stored hash may reconstruct to an open
   term and the value inherits that. The repair is a "references denote closed
   terms" premise, which must also travel in the conclusion.
2. **The `Eval` transition was missing `closed_Σ(h)`.** Without it `thm:wf`'s
   Eval case cannot appeal to the (repaired) lemma, so it simply does not hold.
   Costs nothing operationally: a named hash is closed by coherence.
3. **`thm:mono` tacitly assumes the store is keyed by hash.** No clause of
   `def:wf` says so; carried here as the `keyed` coherence clause.
4. **`thm:stability`'s two `wf` premises are unnecessary**, and
   **`prop:elab-print`'s side condition is redundant**. The latter matters:
   `Name` is abstract, so there is no fresh-name supply and the paper's sketch
   ("choosing binder names") could not be carried out — but the printer can
   return the surface term it started from.

## What the Unison read changed

`formalism/open-questions.md` §"Divergences from Unison" (a source read at `db60ce2`) names
eight forks where the two systems chose differently. Three land on the Agda:

- **`cor:transfer` is new**, and it is `thm:stability`'s proof verbatim. Stated only for store
  *growth*, that theorem reads as a fact about one user's timeline; the same proof gives the
  sharing property a commons needs, since immutability does not care whose store the entries sit
  in. Unison relies on it to sync test results across codebases.
- **`thm:alpha` says what it withholds.** Being a biconditional on terms, it keeps `ref h`
  distinct from what `h` reconstructs to — so inlining is *not* hash-preserving here, the
  opposite of Unison's choice, where a reference contributes the referent's hash to its parent.
  The paper now names the fork (`rem:alpha-only`) instead of leaving it implicit.
- **`prop:print-elab` is the sharpest external comparison available.** Unison's `update` renders
  dependents to source, re-parses, and re-typechecks — self-described as *"the world's weirdest
  implementation of AST substitution"* — which is correct only if printing then parsing preserves
  hashes. Unison does not state that property; it tests it over a regression corpus.
  `prop:print-elab` is that property, proved. (Two caveats before leaning on it, recorded in
  open-questions: it is stated for arbor's languages, and arbor's own `Migrate` is a structural
  rewrite that needs no round-trip lemma.)

What did **not** change: the store's key type. Unison hashes a whole SCC, so a definition is
`(Hash, Pos)` — and had arbor followed, `Store`, `wf`, `callers`, ρ and `thm:alpha` would all
have moved. The design doc's own conclusion is that closing the cycle with a binder in the
object language leaves `Σ : Hash ⇀ Node` untouched and "no metatheory in this paper changes
shape", and p19 now supplies the prototype that option was waiting on.

## Layout

```
Everything.agda            the --safe check target
Arbor/
  Prelude.agda             finite partial maps: update, ⊑, growth lemmas
  Hash.agda                HashModel — the paper's (★) as a record field
  NodeSig.agda             the graph-layer signature (see "reuse boundary")
  Core/
    Params.agda            everything assumed, bundled: HashModel, Name, Time
    Syntax.agda            Term, ⊢ₙ, shift/subst/beta, refsT
    Surface.agda           surface terms — split out to keep Term name-free
    Node.agda              the shallow node + its NodeSig instance
    Store.agda             Σ, ingest, ⇝, refs, wf, HashKeyed, thm:alpha
    Eval.agda              Value, ⇓, determinism, ⇓-mono, fueled eval
    Naming.agda            N, resolve, Elab, Print, round-trip, elab-func
    Cache.agda             E and Cache-sound
    History.agda           H, events, HistCoherent, orphans, versions
    Migrate.agda           Scope, substRefs, RewriteData, multiRebind
    Config.agda            Config, Coherent, the six transitions, NameEdit
    Preservation.agda      thm:wf — ingest-wf, the naming/history lemmas
    Rewrite.agda           M3 in progress: Finite, ρ by well-founded recursion
    Meta.agda              the paper, mirrored
    Model.agda             a consistency witness for HashModel
    Counterexamples.agda   the refutation of lem:closed-no-stuck as first stated
```

Each `Core` module takes a single `(P : Params)` and re-exports the previous one
publicly, so the chain accumulates and imports stay short.

## Representation choices

Recorded with rationale in `../decisions.md` (2026-08-05). In brief:

- **Extrinsic scoping**, reversing this file's earlier lean. `Term` is not
  indexed; scoping is the paper's inductive relation `⊢ₙ t` and closedness is
  `⊢₀ t`, carried as an explicit proof exactly where the paper carries it as a
  premise. Forced by shallow storage: `dom(Σ)` holds open subterm nodes
  (`rem:nosort`), so an indexed `Term : ℕ → Set` would make `reconstruct` return
  a level existential that is not even canonical. α-equivalence stays
  definitional regardless — that comes from de Bruijn, not from the indexing.
  Cost: `beta`-preserves-closedness is an explicit lemma (proved, M2).

- **The paper's partial functions are relations.** `reconstruct` is `σ ⊢ h ⇝ t`,
  with `⇝-func` recovering the partial-function reading; the migration rewrite ρ
  will be the same. As Agda functions they would need termination arguments that
  do not exist in general — a store whose structural children loop is a
  perfectly good value of type `Store`. This is the move the paper already made
  for printing and evaluation. The fueled `eval`/`recon` are separate, total,
  and are what `cor:fuel` is about.

- **`Σ : Hash → Maybe Node`**, a function with decidable key equality, not an
  association list or `Data.AVL`. Inclusion is then literally "everything the
  smaller map says, the bigger map says too", which is what `thm:mono` and
  `thm:stability` are stated in terms of. A finite `support` field joins the
  record when M3's rewrite has to enumerate `dom(Σ)` — not before.

- **`HashModel` is parameterized over the entry functor.** A record with both
  `Hash : Set` and `hash : Node → Hash` as fields is circular, since a node's
  children are hashes. `data Node (H : Set)` breaks the knot.

## The reuse boundary for arbor-stlc

`arbor-stlc.tex` §"Mechanization note" asked for this decision to be recorded
when the work began; it is `Arbor/NodeSig.agda`. Everything the paper says about
the store *as a graph* — `wf`, callers, the migration rewrite — touches an entry
only through its structural children, its reference children, and a map over
both. So that is a record, and `Arbor.Core.Node` instantiates it. arbor-stlc's
two-sorted entry (`def:entry`) becomes a second instance rather than a fork.

`Config` and `_⟶_` are deliberately **not** generic: arbor-stlc adds a `TypeOf`
transition, so abstracting there would buy nothing.

## Milestones

- **M1 — identity, naming, stability.** Done.
- **M2 — well-formedness and safety.** Done: `thm:wf`, `thm:histcoh`,
  `cor:fuel`, `lem:closed-no-stuck`, `prop:elab-print`.
- **M3 — migration.** Started; `Arbor/Core/Rewrite.agda`.
  - **Done:** `Finite σ` (support list + completeness) as a *separate*
    assumption rather than a change to `Store`, so finiteness is assumed only
    where the paper needs it; a dependent
    `substRefsD : (t : Term) → (∀ r → r ∈ refsT t → Hash) → Term`, so the
    recursive call at a reference is justified by the membership proof that
    makes its target a predecessor; **ρ itself**, by `Acc` recursion on
    `Store.Acyclic`, with `def:cascade`'s seed clause discharged; and
    **`rho-irr`**, ρ's independence from the accessibility proof handed to it —
    without which ρ is a recipe rather than a function and no property of its
    *values* is statable. `Acc` is propositional only up to funext, which
    `--safe` does not provide, so this is a double `Acc` induction resting on
    `substRefsD-cong` (the two step functions agree only pointwise).

    Also `substRefsD-refs`: every reference of a rewritten term is the image of
    a reference of the original. That is the provenance fact clause (iii) turns
    on — an edge out of a rewritten entry cannot point anywhere the original
    did not.

    One transcription note worth keeping: ρ takes the seed decision and the
    store lookup as *arguments* rather than `with`-scrutinees. A `with` compiles
    to an opaque generated function, and every property of ρ then becomes an
    ill-typed with-abstraction — which is what happened on the first attempt.

    Worth noting how ρ became definable: the paper defines it "by recursion on
    the combined structural+reference graph", but separating the halves avoids
    needing a combined-graph well-foundedness argument at all. The structural
    half is ordinary structural recursion on the *reconstruction* (a `Term`),
    and only the reference half needs well-foundedness — which `wf` (iii)
    already supplies in exactly the right shape.
  - **Remaining, and where it is genuinely hard:**
    1. *Registration.* Σ′ must extend Σ by every node in ρ#'s image: a fold over
       `Finite.support`, plus independence from the support list chosen.
       Mechanical, not short.
    2. *`wf` for Σ′* — the wall. Clauses (i) and (ii) follow
       `Preservation.ingest-recon` / `ingest-tgt`. Clause (iii) does not:
       **ρ is not injective**, so acyclicity does not transfer along it. Two
       entries can rewrite to the same hash — that is the point of
       content-addressing — so a cycle among ρ-images need not pull back to a
       cycle in Σ. The paper's argument is the registration *order* ("ordering
       new nodes by registration extends any topological order of the old
       reference graph"), so the fold in (1) has to run in a dependency order
       (`def:deporder`) and the proof has to read that order back out. That is
       the same machinery `lem:cascade-order` is about; the two land together.
    3. *The defining equation.* `ρ-at : ¬ (h ≡ gold) → σ ⊢ h ⇝ t →
       ρ h ≡ hashOf (substRefsD t (λ r _ → if guarded r then ρ r else r))` —
       what turns ρ from a recursion into an equation, and what everything
       downstream should cite instead of `rho`. All three ingredients are now
       in place: `rho-irr`, `⇝-func`, `substRefsD-cong`.
- **M4 — arbor-stlc.** A second `NodeSig` instance, then typing, Θ, the
  residual, and the clean oracle.

## Keeping the paper and the proofs together

`make labels` diffs `\label{}`s in a paper against `(label)` mentions in the
Agda sources. The convention is that every statement and definition in the paper
is named in a comment somewhere in `Arbor/`.

```sh
sh ../scripts/check-labels.sh                          # arbor-core: 0 missing
sh ../scripts/check-labels.sh ../paper/arbor-stlc.tex   # 27 missing, until M4
```

This exists because of how the drift happened: `agda/README.md` sat unchanged
across the whole arbor-stlc landing, and both it and `decisions.md` went on
indexing theorems as `T1`–`T8`, a numbering the papers had stopped using. Paper
labels are now the only names.

Caveat when running it against `arbor-stlc.tex`: that paper is a *delta*
document and reuses label names (`def:core`, `def:ingest`, `thm:wf`, …) for its
own restated versions, so the handful it reports as mirrored are coincidental
name matches against arbor-core's, not coverage.
