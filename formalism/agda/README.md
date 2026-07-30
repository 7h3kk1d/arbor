# arbor-core — Agda mechanization roadmap

No code yet. This file records how `paper/arbor-core.tex` is meant to map onto a machine-checked
development, so the eventual `.agda` modules are a transcription rather than a redesign. The
paper's definitions and theorems are already shaped for this (see the paper's §"Mechanization
roadmap").

## Target

A `--safe` Agda development (Agda 2.6.x, `agda-stdlib`) proving the paper's theorems
T1–T8 (`thm:alpha`, `thm:nsb`, `thm:mono`, `thm:wf`, `thm:stability`, `thm:histcoh`,
`thm:migosc`, and the corollaries on caching / incrementality / fuel).

## Representation choices

- **Syntax — intrinsic, well-scoped de Bruijn.**
  `Term : ℕ → Set` with variables `Fin n`, one constructor per core form:
  ```
  data Term (n : ℕ) : Set where
    var : Fin n → Term n
    lam : Term (suc n) → Term n
    app : Term n → Term n → Term n
    ref : Hash → Term n
  ```
  Closedness = `Term 0` (a *type*, not a predicate); α-equivalence is definitional. This
  discharges Def. `def:closed` and the `⇐` half of T1 (`thm:alpha`) by typing. `shift`/`subst`/
  `beta` follow the standard `Fin`-arithmetic presentations (McBride/Allais); `ref` is inert.

- **Hash — an abstract record (one postulate).**
  ```
  record HashModel : Set₁ where
    field Hash    : Set
          _≟_     : Decidable (_≡_ {A = Hash})
          hash    : Node → Hash
          hash-inj : ∀ {m n} → hash m ≡ hash n → m ≡ n
  ```
  The paper's axiom (★) is exactly `hash-inj`. Parameterize the whole development over a
  `HashModel` so the injectivity assumption is named and isolated.
  - *Rejected alternative:* `Hash := Term 0`, `hash := id` (injective by `refl`, zero
    postulates) — collapses `ref` back to inlining and makes the store semantically inert,
    weakening T2/T5. Kept out; see `../decisions.md`.

- **Stores — finite maps with decidable membership.**
  `Σ : Hash ⇀ Node`, `N : Name ⇀ Hash`, `E : Hash ⇀ Hash` (cache values are ingested store
  terms), `H : Name ⇀ List (Maybe Hash × Time)`.
  Start with association lists keyed by decidable equality (simplest proofs); move to
  `Data.AVL` only if membership/`wf` proofs get heavy. `wf Σ` as a decidable predicate
  (no dangling child, reference targets closed, reference graph acyclic — all decidable on
  finite maps; no definition sort, per decisions.md 2026-07-30). Configuration coherence
  (`wf`, plus names and history hashes closed in `Σ`) likewise.

- **Evaluation — fuel-indexed, plus the relation.**
  ```
  data Result : Set where value stuck steplimit : Hash → Result
  eval : ℕ → Σ → Hash → Σ × Result   -- hash-level; ingests β-reducts and results, as p4 does
  ```
  The inductive `_⊢_⇓_` stays term-level and pure (it never grows `Σ`); a soundness lemma links
  the two, and `value`-results are exactly what the `Eval` transition writes into `E`. Prove
  **fuel-monotonicity** (`eval k = value v → eval (suc k) = value v`) — this is Corollary
  `cor:fuel` and the formal reason `StepLimit` is uncacheable but `Value`/`Stuck` are.

- **Elaboration and printing — a pair of inductive judgments.**
  `Γ;N ⊢ s ⇛ t` (a partial function: least-index bound rule, disjoint free rule) and
  `Δ;N ⊢ t ⇚ s` (deliberately relational: any binder names, any aliases). Round-trip theorems:
  print-then-elaborate is exact (paper's `prop:print-elab`); elaborate-then-print holds up to
  `≈` = kernel of elaboration (`prop:elab-print`), characterized by binder renaming + alias
  swap.

- **Transitions — an inductive relation.**
  `data _⟶_ : Config → Config → Set` with one constructor per rule
  (`Ingest`, `Eval`, `Bind`, `Rebind`, `Unbind`, `Migrate`). Constructor premises carry the
  paper's guards (`Ingest`: closedness — free with `Term 0` — plus reference targets closed;
  `Bind`/`Rebind`: target closed); coherence preservation is then an induction over `_⟶_`.
  Theorems are statements about `_⟶_` (single step) and its reflexive-transitive closure
  (whole histories). The `Migrate` case defines the whole-store rewrite `ρ` by recursion on
  the acyclic combined structural+reference graph — fuel bounded by `|dom Σ|` works — and the
  sequential-pass lemma (paper's `lem:cascade-order`) bridges to implementation-style
  cascades.

## Proof staging

1. **Milestone 1 — identity & store** (T1–T4): `thm:alpha` (via `hash-inj`), `thm:nsb`
   (immediate: `_⊢_⇓_` does not mention `N`), `thm:mono`, `thm:wf`.
2. **Milestone 2 — evaluation & migration** (T5–T8 + corollaries): `thm:stability` (reachable
   entries unchanged under `⊆`), cache soundness / incrementality, `thm:histcoh`, `thm:migosc`.

## Open modeling questions

Tracked in `../open-questions.md` (store representation; intrinsic vs. extrinsic scoping;
whether `stuck` is reachable on closed terms and can be dropped; how much to mechanize first).

## Layout (when code lands)

```
agda/
  Arbor/Syntax.agda        -- Term, shift/subst/beta
  Arbor/Hash.agda          -- HashModel record
  Arbor/Store.agda         -- Node, Σ, ingest/reconstruct, wf
  Arbor/Eval.agda          -- eval (fueled) + _⊢_⇓_ + monotonicity
  Arbor/Naming.agda        -- N, resolve, elaboration, printing + round-trips
  Arbor/Cache.agda         -- E and its discipline
  Arbor/Edit.agda          -- Config, _⟶_, migration
  Arbor/Meta.agda          -- T1–T8
  arbor-core.agda-lib
```
