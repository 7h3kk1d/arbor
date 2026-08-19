{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- The paper, mirrored
--
-- Every numbered statement of paper/arbor-core.tex appears below as a named
-- Agda TYPE whose name is the paper's label (thm:nsb ↦ Thm-nsb). A statement
-- is *proved* exactly when that type has an inhabitant, named in lower case
-- (thm-nsb). A statement not yet proved is a type with no inhabitant.
--
-- Why types rather than postulates: Agda's --safe rejects postulates, and that
-- is the right pressure. This library assumes NOTHING — the paper's one axiom
-- (★) is a field of Arbor.Hash.HashModel, and Arbor.Core.Model exhibits an
-- inhabitant of that record, so no statement here is vacuous. An open
-- obligation cannot be silently leaned on by a later proof, because there is
-- no term to lean on.
--
-- Status markers are machine-read by ../scripts/status.sh; the label in
-- parentheses is machine-read by ../scripts/check-labels.sh. Keep both.
--
--   [proved]           an inhabitant is given below
--   [by-construction]  discharged by a datatype or module declaration
--   [spec, Mn]         discharged from a specification record; the obligation
--                      transfers to milestone n's construction of an inhabitant
--   [open, Mn]         stated only; scheduled for milestone n
--   [deferred, Mn]     not yet statable; needs a construction from milestone n
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Meta (P : Params) where

open import Arbor.Core.Preservation P public

open import Arbor.Hash using (HashModel)
open HashModel hashModel using (_≟_)
open import Arbor.Prelude using (just-inj)

open import Data.List.Base using (List; length)
open import Data.List.Membership.Propositional using (_∈_; _∉_)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Nat.Base using (ℕ; suc)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Relation.Binary.PropositionalEquality
  using (_≡_; refl; sym; trans; cong; subst)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)

------------------------------------------------------------------------
-- Part I–II — identity
------------------------------------------------------------------------

-- (thm:alpha) [proved] — Hashing collapses exactly α-equivalence
--
-- "For core terms t,u, ingesting yields the same hash iff t = u (which, since
-- core is de Bruijn, is α-equivalence of the surface terms they elaborate
-- from)."
--
-- Stated on hashOf, the store-independent root; Thm-alpha-roots is the bridge
-- to the paper's phrasing in terms of ingest. Being a biconditional on TERMS,
-- it also says what it withholds (rem:alpha-only): equal hashes force equal
-- core terms, so `ref h` and the term h reconstructs to stay distinct and
-- inlining a reference is not hash-preserving — the opposite of Unison's
-- choice, and a fork the paper now names rather than leaves implicit. The (⇐) half is `cong`, which is
-- the mechanized form of "ingest is a function"; the (⇒) half is the
-- injectivity induction.
Thm-alpha : Set
Thm-alpha = ∀ t u → (hashOf t ≡ hashOf u → t ≡ u)
                  × (t ≡ u → hashOf t ≡ hashOf u)

thm-alpha : Thm-alpha
thm-alpha t u = hashOf-inj t u , cong hashOf

Thm-alpha-roots : Set
Thm-alpha-roots = ∀ t σ → proj₂ (ingest σ t) ≡ hashOf t

thm-alpha-roots : Thm-alpha-roots
thm-alpha-roots = ingest-root

-- (prop:namefree) [by-construction] — Names never enter the store
--
-- "If Γ;N ⊢ s ⇛ t then t is a core term, which has no name leaf. Hence every
-- ingested definition is name-free."
--
-- There is nothing to prove: Arbor.Core.Syntax takes only `Hash` as a module
-- parameter and its Term has no Name-typed constructor, while surface syntax
-- lives in the separately-parameterized Arbor.Core.Surface. The module
-- structure IS the proof, which is the paper's own point — "the split is what
-- makes 'names never appear in stored programs' a statement about *types*, not
-- a runtime check."

------------------------------------------------------------------------
-- Part II — evaluation
------------------------------------------------------------------------

-- (lem:determinism) [proved] — Determinism
Lem-determinism : Set
Lem-determinism = ∀ {σ t v₁ v₂} → σ ⊢ t ⇓ v₁ → σ ⊢ t ⇓ v₂ → v₁ ≡ v₂

lem-determinism : Lem-determinism
lem-determinism = ⇓-det

-- (lem:closed-no-stuck) [proved] — Closed terms never get stuck at the head
--
-- "A closed weak-head normal form has an abstraction at its head", so the
-- missing Var rule loses no closed-term behaviour. Stated as the consequence
-- thm:wf's Eval case needs: evaluation preserves closedness.
--
-- FINDING (2026-08-05): the paper's premises — wf(Σ) and closed(t) — do not
-- suffice, and the statement they give is FALSE. `closed (ref h)` holds for
-- every h, while wf constrains only the references of *stored* entries, so h
-- may reconstruct to an open term and the value inherits that.
-- Arbor.Core.Counterexamples.paper-statement-is-false refutes it outright, with
-- wf(Σ) discharged for an explicit two-entry store — not assumed.
--
-- The repair is one premise, RefsClosed: t's references denote closed terms.
-- It is exactly Ingest's second premise and coherence clause (b). The
-- conclusion must carry RefsClosed as well, because the App case reduces to
-- `beta t₀ vu` and needs the references of the value it received. And the Eval
-- transition needs closed_Σ(h) added to it, which is where the missing premise
-- actually bit (Config.t-eval).
Lem-closed-no-stuck : Set
Lem-closed-no-stuck = ∀ {σ t v} → WF σ → Closed t → RefsClosed σ t →
                      σ ⊢ t ⇓ v → Closed v × RefsClosed σ v

lem-closed-no-stuck : Lem-closed-no-stuck
lem-closed-no-stuck = ⇓-closed

-- Its workhorse, and the one real cost of extrinsic scoping: under intrinsic
-- indexing this would be a typing fact (decisions.md 2026-08-05).
Beta-closed : Set
Beta-closed = ∀ {t v} → 1 ⊢ t → Closed v → Closed (beta t v)

beta-closed′ : Beta-closed
beta-closed′ = beta-closed

------------------------------------------------------------------------
-- Part III — naming
------------------------------------------------------------------------

-- (lem:elab-premises) [proved] — Elaboration discharges the ingest premises
--
-- "A top-level elaboration produces a closed term all of whose references lie
-- in ran(N) — these are exactly the premises of the Ingest transition."
Lem-elab-premises : Set
Lem-elab-premises = ∀ {ν Γ s t} → Elab ν Γ s t →
                    (length Γ ⊢ t) × (∀ {r} → r ∈ refsT t → MN._∈ran_ r ν)

lem-elab-premises : Lem-elab-premises
lem-elab-premises d = elab-scoped d , elab-refs d

-- (prop:print-elab) [proved] — Printing inverts to elaboration
--
-- "Whatever rendering the printer chooses, re-resolving it recovers exactly
-- the stored term."
Prop-print-elab : Set
Prop-print-elab = ∀ {ν Δ t s} → Print ν Δ t s → Elab ν Δ s t

prop-print-elab : Prop-print-elab
prop-print-elab = print-elab

-- (prop:elab-print) [proved] — Elaboration round-trip, up to name choice
--
-- "If Γ;N ⊢ s ⇛ t ... then some s′ has Γ;N ⊢ t ⇐ s′, and every such s′
-- satisfies s ≈ s′", where ≈ is the kernel of elaboration — s and s′ elaborate
-- to the same core term. Both halves below: existence, and that any printing
-- re-elaborates to the same t (which, with the given derivation, IS s ≈ s′).
--
-- FINDING (2026-08-05): the paper's side condition — "every reference in t has
-- a name outside Γ" — is redundant, since an Elab derivation witnesses it at
-- every el-free leaf. That matters more than it sounds: Name is abstract here,
-- so there is no fresh-name supply, and the paper's sketch ("choosing binder
-- names away from the finitely many aliases the subterm's references need")
-- could not be carried out. Returning s itself sidesteps the choice entirely.
Prop-elab-print : Set
Prop-elab-print = ∀ {ν Γ s t} → Elab ν Γ s t →
                  ∃[ s′ ] ((Print ν Γ t s′)
                         × (∀ {s″} → Print ν Γ t s″ → Elab ν Γ s″ t))

prop-elab-print : Prop-elab-print
prop-elab-print {s = s} d = s , elab-print d , print-elab

------------------------------------------------------------------------
-- Part V — the edit calculus
------------------------------------------------------------------------

-- (thm:nsb) [proved] — No silent breakage
--
-- "If C ⟶ C′ by Bind, Rebind, or Unbind, then Σ is unchanged, and therefore
-- for every h, Σ ⊢ ref h ⇓ v holds after the edit iff it held before. A name
-- edit changes only *which* hash a name resolves to, never what any referenced
-- hash computes."
--
-- The proof is `refl` three times. That is the theorem's real content: ⇓ is
-- indexed by Σ alone, so its extension is not merely preserved but literally
-- the same relation. Note this survives the presence of a Migrate rule in
-- _⟶_ — Migrate is simply not one of the three name edits.
Thm-nsb : Set
Thm-nsb = ∀ {C C′} → NameEdit C C′ → store C ≡ store C′

thm-nsb : Thm-nsb
thm-nsb (ne-bind _ _) = refl
thm-nsb (ne-rebind _) = refl
thm-nsb (ne-unbind _) = refl

Thm-nsb-eval : Set
Thm-nsb-eval = ∀ {C C′ h v} → NameEdit C C′ →
               (store C ⊢ ref h ⇓ v) → (store C′ ⊢ ref h ⇓ v)

thm-nsb-eval : Thm-nsb-eval
thm-nsb-eval {h = h} {v = v} ne d =
  subst (λ σ → σ ⊢ ref h ⇓ v) (thm-nsb ne) d

-- (thm:mono) [proved] — Monotonicity and immutability
--
-- "Every transition has Σ ⊆ Σ′ (as partial functions), and no existing entry
-- h ↦ n is ever overwritten."
--
-- With the store modelled as Hash → Maybe Node, the two halves are one
-- statement: if an entry had been overwritten, the old lookup would no longer
-- be reported. The Coherent premise supplies HashKeyed — see the note under
-- Def-wf.
Thm-mono : Set
Thm-mono = ∀ {C C′} → Coherent C → C ⟶ C′ → store C ⊑ store C′

thm-mono : Thm-mono
thm-mono coh (t-ingest {σ = σ} {t = t} _ _) = ingest-⊑ t σ (Coherent.keyed coh)
thm-mono coh (t-eval {σ = σ} {v = v} _ _ _) = ingest-⊑ v σ (Coherent.keyed coh)
thm-mono coh (t-bind _ _)                   = ⊑-refl
thm-mono coh (t-rebind _)                   = ⊑-refl
thm-mono coh (t-unbind _)                   = ⊑-refl
thm-mono coh (t-migrate R _ _ _ _ _)        = RewriteData.grows R

-- (thm:wf) [proved] — Coherence preservation
--
-- Clause by clause, as the paper's sketch says. The substantial parts are all
-- in Arbor.Core.Preservation: ingest-recon / ingest-tgt for wf (i) and (ii),
-- and for (iii) the acyclicity argument — "ordering new nodes by registration
-- extends any topological order of the old reference graph, so no cycle is
-- created" — which is Store.acyclic-transfer fed by ingest-edges.
--
-- The Migrate case is discharged from RewriteData.wf′, i.e. from the
-- specification a migration rewrite must satisfy; see Thm-migosc.
Thm-wf : Set
Thm-wf = ∀ {C C′} → Coherent C → C ⟶ C′ → Coherent C′

thm-wf : Thm-wf
thm-wf = coherent-preserved

-- (thm:stability) [proved] — Evaluation stability under growth
--
-- "Suppose wf(Σ), Σ ⊆ Σ′, and wf(Σ′). Then for every h, Σ ⊢ ref h ⇓ v implies
-- Σ′ ⊢ ref h ⇓ v."
--
-- FINDING (2026-08-05): the two wf premises are not needed. The proof replays
-- the derivation using only immutability of the entries it reads, which is
-- Σ ⊆ Σ′ alone. Thm-stability-strong is what is actually proved; Thm-stability
-- is the paper's statement, derived from it by discarding two arguments. See
-- open-questions.md.
Thm-stability : Set
Thm-stability = ∀ {σ σ′} → WF σ → σ ⊑ σ′ → WF σ′ →
                ∀ {h v} → σ ⊢ ref h ⇓ v → σ′ ⊢ ref h ⇓ v

thm-stability : Thm-stability
thm-stability _ sub _ d = ⇓-mono sub d

Thm-stability-strong : Set
Thm-stability-strong = ∀ {σ σ′ t v} → σ ⊑ σ′ → σ ⊢ t ⇓ v → σ′ ⊢ t ⇓ v

thm-stability-strong : Thm-stability-strong
thm-stability-strong = ⇓-mono

-- (cor:cache) [proved] — Cache soundness / no re-evaluation
--
-- "If Cache-sound holds for C and C ⟶ C′, it holds for C′: a written entry
-- stays valid in every future store. Hence a term is evaluated at most once
-- across the whole history — the cache never needs invalidation."
Cor-cache : Set
Cor-cache = ∀ {C C′} → Coherent C → C ⟶ C′ → CacheSound (store C′) (cache C′)

cor-cache : Cor-cache
cor-cache = cache-preserved

-- (cor:transfer) [proved] — Stability under transfer
--
-- "Let Σ₀ be a store fragment shared by two configurations, and Σ any store with
-- Σ₀ ⊆ Σ. If Σ₀ ⊢ ref h ⇓ v then Σ ⊢ ref h ⇓ v." The point is the quantifier,
-- not the proof: Σ need not be a temporal successor of Σ₀, so two codebases
-- holding the same fragment agree on what it computes, without either
-- containing the other.
--
-- It is thm:stability's proof verbatim — and that IS the content. ⇓-mono
-- replays a derivation over entries fixed by immutability, and immutability
-- does not care whose store the entries sit in. Nothing had to be strengthened
-- to get the sharing property; it was already there, stated too narrowly. This
-- is the form the computational-commons argument uses, and the form Unison
-- relies on when it syncs test results across codebases.
Cor-transfer : Set
Cor-transfer = ∀ {σ₀ σ h v} → σ₀ ⊑ σ → σ₀ ⊢ ref h ⇓ v → σ ⊢ ref h ⇓ v

cor-transfer : Cor-transfer
cor-transfer = ⇓-mono

-- (thm:histcoh) [proved] — History coherence
--
-- "x ∈ dom(N) with N(x)=h iff the head of H(x) is (Some h,_); and x ∉ dom(N)
-- iff H(x) is empty or has a ⊥ head." Both directions are the single clause
-- HistCoherent, since ⊥, "no events", and "unbound" are all `nothing`. A
-- consequence of Thm-wf once hist-coh is a coherence clause, but stated
-- separately as the paper does.
Thm-histcoh : Set
Thm-histcoh = ∀ {C C′} → Coherent C → C ⟶ C′ →
              HistCoherent (names C′) (hist C′)

thm-histcoh : Thm-histcoh
thm-histcoh coh st = Coherent.hist-coh (thm-wf coh st)

-- (cor:fuel) [proved] — Only divergence is non-cacheable
--
-- "eval_k(t) = v ⟹ eval_{k+1}(t) = v (fuel-monotonicity), and eval_k(t) = v
-- for some k iff Σ ⊢ t ⇓ v. So StepLimit is precisely the non-stable outcome
-- that rem:nostepcache forbids caching, while Value/Stuck are stable."
--
-- The three parts together say: Value is stable under increasing fuel, and
-- reachable at SOME fuel exactly when ⇓ is defined. So StepLimit — "no
-- derivation *yet*" — is the one outcome a larger budget can overturn, which is
-- precisely why rem:nostepcache forbids caching it while Value/Stuck are fair
-- game. p4's default_step_limit = 10000 is thereby an implementation detail
-- with no semantic content.
Cor-fuel : Set
Cor-fuel = (∀ k σ t v → eval k σ t ≡ value v → eval (suc k) σ t ≡ value v)
         × (∀ {k σ t v} → eval k σ t ≡ value v → σ ⊢ t ⇓ v)
         × (∀ {σ t v} → σ ⊢ t ⇓ v → ∃[ k ] (eval k σ t ≡ value v))

cor-fuel : Cor-fuel
cor-fuel = (λ k σ t v → eval-mono k σ t)
         , (λ {k} {σ} {t} → eval-sound k σ t)
         , eval-complete

------------------------------------------------------------------------
-- Part V — migration
------------------------------------------------------------------------

-- (cor:incremental) [proved] — Migration is incremental
--
-- "After a Migrate step, every cache entry for a hash in the OLD dom(Σ)
-- remains valid; the set of terms that may require (re-)evaluation is
-- contained in the newly-registered hashes."
--
-- The first half is the whole mathematical content, and it needs only that the
-- store grew: an entry's validity is a fact about hashes, and migration adds
-- hashes rather than changing them. The h ∈ dom(Σ) premise is stated because
-- the paper states it; the proof does not use it, since Cache-sound already
-- pins hv and h into Σ. The second half ("only new hashes may need
-- re-evaluation") is the contrapositive read off the same fact.
Cor-incremental : Set
Cor-incremental = ∀ {σ ε gold gnew sc} (R : RewriteData σ gold gnew sc) →
                  CacheSound σ ε →
                  ∀ {h hv} → ε h ≡ just hv → h ∈dom σ →
                  ∃[ v ] ((RewriteData.σ′ R ⊢ hv ⇝ v)
                        × (RewriteData.σ′ R ⊢ ref h ⇓ v))

cor-incremental : Cor-incremental
cor-incremental R cs eq _ = CacheSound-mono (RewriteData.grows R) cs eq

-- (thm:migosc) [spec, M3] — Migration atomicity and scope
--
-- The paper's three clauses land differently, which is why only one type is
-- given here:
--
--   (a) Atomicity is already discharged by construction. Config.t-migrate
--       carries the validation of U as a premise, so a bundle that fails it
--       yields no step at all and ⟨N,H⟩ cannot advance, while Σ is extended by
--       RewriteData.σ′ regardless — "only ⟨N,H⟩ is transactional; Σ is
--       accumulate-only". In this untyped rung validation cannot fail at all
--       (rem:gate), so the contract is vacuous here and stated for the typed
--       successor's gate.
--
--   (b) Scope — "the rewritten entries are exactly those with ρ(h) ≠ h" —
--       cannot yet be stated: it quantifies over the *construction* of ρ, and
--       Arbor.Core.Migrate.RewriteData only specifies it. Writing a weaker
--       statement here would read as coverage it does not have; M3 adds the
--       node-level recursion and the fixed-outside-scope field, and clause (b)
--       is stated then.
--
--   (c) Preservation is the type below, and is discharged from the
--       RewriteData specification. That is not a proof dressed up as one: the
--       obligation transfers wholesale to M3's construction of an inhabitant,
--       which must produce this field from ρ's definition. It is a field rather
--       than an open statement because thm:wf quantifies over ALL transitions,
--       Migrate included, and would otherwise have been unprovable until M3.
Thm-migosc : Set
Thm-migosc = ∀ {σ gold gnew sc} (R : RewriteData σ gold gnew sc) →
             WF σ → HashKeyed σ → WF (RewriteData.σ′ R)

thm-migosc : Thm-migosc
thm-migosc R wf kd = RewriteData.wf′ R wf kd

-- (lem:cascade-order) [deferred, M3] — Sequential passes compute ρ
--
-- Deferred for clause (b)'s reason: the statement quantifies over "rewriting
-- each entry's reconstruction with the substitution accumulated so far and
-- re-ingesting", i.e. over a sequential pass that does not exist until ρ is
-- constructed. Arbor.Core.Migrate.DependencyOrder and Store.ingest-⇝ are the
-- pieces already in place for it.
