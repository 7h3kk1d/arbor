# arbor-core — Agda mechanization

Machine-checked transcription of `paper/arbor-core.tex`. **Milestones 1–3 are proved: 17 of the
paper's 18 statements.** Only `lem:cascade-order` remains, deferred. Every statement is stated in
Agda whether or not it is proved, so drift between paper and proofs is visible.

```sh
make check    # agda --safe Everything.agda
make status   # what is proved, what is open, per paper label
make labels   # paper labels with no counterpart in the Agda sources
```

Toolchain: **Agda 2.7.0**, **agda-stdlib 2.1**. The stdlib is resolved through the machine-local
`~/.agda/libraries`, which points at `~/Projects/plfa/standard-library`. `arbor-core.agda-lib`
declares `depend: standard-library` and does not care where it lives.

## What `--safe` buys, and what it does not

`make check` passes under `--safe` with **no postulates, no `TERMINATING` pragmas, and no unsolved
metas**. The paper's single axiom (★, `def:hash`) is a field of the `HashModel` record the development
is parameterized over, and `Arbor.Core.Model` exhibits an inhabitant of that record. Nothing is
assumed and no proved statement is vacuous.

A green build does not mean the paper is proved. Unproved statements appear in `Arbor/Core/Meta.agda`
as named types with no inhabitant. A type with no term cannot be leaned on by a later proof, whereas a
postulate can. `make status` is the progress metric.

    17 of 18 statements discharged.

## Status

`Arbor/Core/Meta.agda` mirrors the paper. Each statement is a type named after the paper's label
(`thm:nsb` ↦ `Thm-nsb`), proved when that type has an inhabitant (`thm-nsb`). Run `make status` for
the live table.

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
| `thm:migosc` — atomicity and scope | `thm-migosc`, `thm-migosc-realized` | proved |
| `lem:cascade-order` | — | deferred, M3 |

`thm:migosc` is discharged from `RewriteData`, the record specifying what a migration rewrite must
deliver. It was made a field, not an open statement, because `thm:wf` quantifies over all transitions,
Migrate included, and would otherwise have been unprovable until M3 (the design call in
`../decisions.md`). The obligation transferred to M3's construction of an inhabitant, `rewriteData`,
which now exists.

Proved beyond the paper because the transcription needed them:
- `⇝-func` and `⇝-mono`: reconstruction is functional and survives store growth, the workhorse of
  `thm:stability`;
- `elab-func`: `def:elab`'s claim that elaboration is a partial function;
- `ingest-⇝`: reconstructing an ingested term returns it, used twice by the paper without being
  stated;
- the scoping lemmas around `beta` that extrinsic scoping makes explicit;
- `acyclic-transfer` and the `ingest-*` family in `Preservation.agda`, which are what `thm:wf`'s
  four-sentence proof sketch expands to.

## What mechanizing found

Four gaps in the paper, all fixed there.

1. **`lem:closed-no-stuck` was false as stated.** `Arbor/Core/Counterexamples.agda` refutes it, with
   `WF σ` proved, not assumed, for an explicit two-entry store. `closed (ref h)` holds for every `h`,
   while `wf` constrains only the references of stored entries, so a stored hash may reconstruct to an
   open term and the value inherits it. The repair is a "references denote closed terms" premise,
   which must also travel in the conclusion.
2. **The `Eval` transition was missing `closed_Σ(h)`.** Without it `thm:wf`'s Eval case cannot appeal
   to the repaired lemma, and does not hold. It costs nothing operationally: a named hash is closed by
   coherence.
3. **`thm:mono` tacitly assumes the store is keyed by hash.** No clause of `def:wf` says so. Carried
   here as the `keyed` coherence clause.
4. **`thm:stability`'s two `wf` premises are unnecessary, and `prop:elab-print`'s side condition is
   redundant.** The latter matters: `Name` is abstract, so there is no fresh-name supply and the
   paper's sketch ("choosing binder names") could not be carried out. The printer can instead return
   the surface term it started from.

## What the Unison read changed

`formalism/open-questions.md` §"Divergences from Unison" (a source read at `db60ce2`) names eight forks
where the two systems chose differently. Three affect the Agda:

- **`cor:transfer` is new**, and its proof is `thm:stability`'s verbatim. Stated only for store
  growth, that theorem reads as a fact about one user's timeline. The same proof gives the sharing
  property a commons needs, since immutability does not depend on whose store holds the entries.
  Unison relies on it to sync test results across codebases.
- **`thm:alpha` says what it withholds.** As a biconditional on terms, it keeps `ref h` distinct from
  what `h` reconstructs to, so inlining is not hash-preserving here. Unison chose the opposite at term
  level: a reference contributes the referent's hash to its parent. The paper names the fork
  (`rem:alpha-only`).
- **`prop:print-elab` is the sharpest external comparison available.** Unison's `update` renders
  dependents to source, re-parses and re-typechecks (self-described as *"the world's weirdest
  implementation of AST substitution"*). That is correct only if printing then parsing preserves
  hashes. Unison does not state the property; it tests it over a regression corpus. `prop:print-elab`
  is that property, proved. Two caveats are recorded in open-questions: it is stated for arbor's
  languages, and arbor's own `Migrate` is a structural rewrite that needs no round-trip lemma.

The store's key type did not change. Unison hashes a whole SCC, so a definition is `(Hash, Pos)`; had
arbor followed, `Store`, `wf`, `callers`, ρ and `thm:alpha` would all have moved. The design doc
concludes that closing the cycle with a binder in the object language leaves `Σ : Hash ⇀ Node`
untouched and "no metatheory in this paper changes shape", and p19 supplies the prototype that option
was waiting on.

## Layout

```
Everything.agda            the --safe check target
Arbor/
  Prelude.agda             finite partial maps: update, ⊑, growth lemmas
  Hash.agda                HashModel — the paper's (★) as a record field
  NodeSig.agda             the retracted graph-layer signature (see "reuse boundary")
  Sig.agda                 the syntax signature: shapes with structural and reference arities
  Generic/                 the store layer over any signature
    Syntax.agda            Node and deep Term over a signature; core pattern synonyms
    Store.agda             store, ingest, reconstruction, refs, wf (Closed as a parameter)
    Preservation.agda      the ingest-* family, generically
    Rewrite.agda           the migration rewrite, generically
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
    Rewrite.agda           M3: Finite, ρ by well-founded recursion, rewriteData
    Meta.agda              the paper, mirrored
    Model.agda             a consistency witness for HashModel
    Counterexamples.agda   the refutation of lem:closed-no-stuck as first stated
  Stlc/                    the arbor-stlc rung (M4), instantiating Generic/
    Node.agda              nine shapes: seven term, two type
    Params.agda            arbor-core's Params at the two-sorted signature
    Syntax.agda            core terms and types as pattern synonyms; sorts, scoping, β
    Store.agda             the two-sorted store, instantiated from Generic/
    Typing.agda            the typing judgment, with T-Ref
    Eval.agda              evaluation (def:value)
    Safety.agda            lem:subst, thm:pres, thm:local
```

Each `Core` module takes a single `(P : Params)` and re-exports the previous one publicly, so the chain
accumulates and imports stay short.

## Representation choices

Recorded with rationale in `../decisions.md` (2026-08-05). In brief:

- **Extrinsic scoping**, reversing this file's earlier lean. `Term` is not indexed. Scoping is the
  paper's inductive relation `⊢ₙ t`, closedness is `⊢₀ t`, and both are carried as explicit proofs
  where the paper carries them as premises. Shallow storage forces this: `dom(Σ)` holds open subterm
  nodes (`rem:nosort`), so an indexed `Term : ℕ → Set` would make `reconstruct` return a non-canonical
  level existential. α-equivalence stays definitional regardless, since it comes from de Bruijn, not
  the indexing. Cost: `beta`-preserves-closedness is an explicit lemma (proved, M2).
- **The paper's partial functions are relations.** `reconstruct` is `σ ⊢ h ⇝ t`, with `⇝-func`
  recovering the partial-function reading, and the migration rewrite is handled the same way. As Agda
  functions they would need termination arguments that do not exist in general: a store whose
  structural children loop is a valid value of type `Store`. The paper already made this move for
  printing and evaluation. The fueled `eval`/`recon` are separate and total, and they are what
  `cor:fuel` is about.
- **`Σ : Hash → Maybe Node`**, a function with decidable key equality, not an association list or
  `Data.AVL`. Inclusion is then "everything the smaller map says, the bigger map says too", which is
  how `thm:mono` and `thm:stability` are stated. Finiteness is a separate `Finite` record, used only
  where M3's rewrite enumerates `dom(Σ)`.
- **`HashModel` is parameterized over the entry functor.** A record with both `Hash : Set` and
  `hash : Node → Hash` as fields is circular, since a node's children are hashes. `data Node (H : Set)`
  breaks the knot.

## The reuse boundary for arbor-stlc — declared, but not yet real

*(Settled 2026-08-07 by construction: `Term` became the fixpoint of a signature (`Arbor/Sig.agda`),
the store layer is proved once in `Arbor/Generic/`, and `Arbor/Stlc/` is a second instance. See
`../open-questions.md` §"The arbor-stlc reuse boundary — settled" and `../decisions.md` 2026-08-07.
The rest of this section records the state before that.)*

`arbor-stlc.tex` asked for this decision to be recorded when the work began, and `decisions.md`
2026-08-05 recorded it as `Arbor/NodeSig.agda`. That was premature. The record exists and
`Arbor/Core/Node.agda` instantiates it, but nothing consumes it. Every graph-layer lemma routes through
`σ ⊢ h ⇝ t`, whose right-hand side is a deep `Term`, so the proofs are shaped by the term language and
not by the node signature. Abstracting nodes was the wrong axis.

Already shared: `Arbor.Prelude` and `Arbor.Hash`, both parameterized. Shareable with a modest
abstraction over `⇝` instead of over nodes: the purely graph-theoretic lemmas `acyclic-transfer`,
`⊑-edges`, `update-dom`, callers. Term-recursive, and otherwise written twice: `ingest`, the `ingest-*`
family, and most of `Preservation`.

M4 had two shapes:

1. **Refactor `Term` as the fixpoint of a functor first**, then share the bulk. Highest payoff, and it
   would make `NodeSig` real, but it touches every proof and the M1–M3 results would all need
   re-checking.
2. **Write `Arbor/Stlc/` as a parallel hierarchy**, sharing only Prelude, Hash and (with the `⇝`
   abstraction) the graph lemmas. Duplicates perhaps 60% of the store layer but leaves 17 proved
   statements untouched.

Config and the transition relation are not generic either way, because arbor-stlc adds a `TypeOf`
transition.

## Milestones

- **M1 — identity, naming, stability.** Done.
- **M2 — well-formedness and safety.** Done: `thm:wf`, `thm:histcoh`, `cor:fuel`,
  `lem:closed-no-stuck`, `prop:elab-print`.
- **M3 — migration.** Done: `Arbor/Core/Rewrite.agda`. ρ is constructed by well-founded recursion on
  `wf` clause (iii), with its seed clause and defining equation (`ρ-at`); then the registration fold;
  all three `wf` clauses for the migrated store; and `rewriteData`, a `RewriteData` inhabitant for any
  finite well-formed store. `thm:migosc` clause (c) is no longer projected from an empty record.

  Two findings revised the plan that preceded them. **The fold needs no dependency order.** Every entry
  of Σ is in `support`, so every image is registered somewhere, and ingest only adds; the reference
  targets are closed in Σ′ whatever order the fold ran in. `ingest-tgt-big` makes this expressible by
  pinning the conclusion to the final store instead of the accumulator. **ρ's non-injectivity is not an
  obstacle.** It defeats the pointwise argument, but the accessibility proof follows an edge forward
  and never chooses a preimage: a colliding hash carries the same node by (★), hence the same
  out-edges.

  Finiteness is the one thing assumed, not derived, and only registration uses it. Hence `Finite` is a
  separate record; changing `def:store` would have rippled through every proof.

- **M4 — arbor-stlc.** Under way in `Arbor/Stlc/` over the generic layer: typing, evaluation and type
  safety so far; Θ, the residual and the clean oracle to follow.

## Keeping the paper and the proofs together

`make labels` diffs a paper's `\label{}`s against `(label)` mentions in the Agda sources. The
convention is that every statement and definition in the paper is named in a comment somewhere in
`Arbor/`.

```sh
sh ../scripts/check-labels.sh                          # arbor-core: 0 missing
sh ../scripts/check-labels.sh ../paper/arbor-stlc.tex   # 22 missing as of 2026-10-20, until M4
```

It exists because `agda/README.md` sat unchanged through the arbor-stlc landing, and both it and
`decisions.md` indexed theorems as `T1`–`T8`, a numbering the papers had stopped using. Paper labels
are now the only names.

Caveat for `arbor-stlc.tex`: it is a delta document and reuses label names (`def:core`, `def:ingest`,
`thm:wf`, …) for its own restated versions, so some labels it reports as mirrored are coincidental
matches against arbor-core's, not coverage.
