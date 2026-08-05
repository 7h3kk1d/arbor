{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- A refutation of lem:closed-no-stuck as the paper states it
--
-- arbor-core lem:closed-no-stuck reads: "If wf(Σ) and closed(t) then no
-- evaluation position reached from t has a free Var at its head", with the
-- operative consequence that evaluation preserves closedness. That is FALSE,
-- and this module proves it false — not merely unproved.
--
-- The gap is that `closed(ref h)` holds for EVERY h (a reference carries no
-- free variable, def:closed's s-ref), while wf constrains only the references
-- OF STORED ENTRIES (clause (ii)); nothing makes a stored hash reconstruct to
-- a closed term, and nothing can, because storage is shallow: dom(Σ) is full
-- of open subterm nodes by design (rem:nosort).
--
-- So take the two-entry store
--
--     Σ = { ⌈nvar 5⌉ ↦ nvar 5 ,  ⌈nlam ⌈nvar 5⌉⌉ ↦ nlam ⌈nvar 5⌉ }
--
-- which is well-formed (nothing in it has a reference, so (ii) and (iii) are
-- vacuous, and both entries reconstruct, so (i) holds). Then `ref h` is closed,
-- it evaluates, and its value is `lam (var 5)` — open.
--
-- The repair, adopted in Arbor.Core.Eval.⇓-closed and stated in Meta.agda: the
-- lemma needs t's references to denote closed terms. That premise is exactly
-- Ingest's second premise and coherence clause (b), so it is available wherever
-- the paper actually uses the lemma — EXCEPT in the Eval transition, which is
-- why def:transitions' Eval rule gains a closed_Σ(h) premise. See
-- ../decisions.md 2026-08-05 and ../open-questions.md.
------------------------------------------------------------------------

module Arbor.Core.Counterexamples where

open import Arbor.Core.Model using (params)
open import Arbor.Core.Eval params

open import Data.Empty using (⊥; ⊥-elim)
open import Data.List.Base using ([])
open import Data.List.Membership.Propositional using (_∈_)
open import Data.Maybe.Base using (just; nothing)
open import Data.Nat.Base using (_<_; s≤s)
open import Data.Product using (_×_; _,_; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Induction.WellFounded using (Acc; acc)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; sym; trans; subst)
open import Relation.Nullary using (¬_)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)
open import Arbor.Prelude using (just-inj)

open import Arbor.Hash using (HashModel)
open HashModel (Arbor.Core.Model.hashModel) using (_≟_)

------------------------------------------------------------------------
-- The store

c : Hash                      -- an OPEN entry: var 5
c = hash (nvar 5)

h : Hash                      -- a lambda whose body is that open entry
h = hash (nlam c)

σ₀ : Store
σ₀ = ∅ [ c ↦ nvar 5 ]

σ : Store
σ = σ₀ [ h ↦ nlam c ]

private
  -- The two hashes are distinct: ⌈nvar 5⌉ = ⌈nlam c⌉ would force
  -- nvar 5 ≡ nlam c by (★).
  c≢h : ¬ (c ≡ h)
  c≢h eq with hash-inj eq
  ... | ()

  lookup-h : σ h ≡ just (nlam c)
  lookup-h = lookup-hit σ₀ h (nlam c)

  lookup-c : σ c ≡ just (nvar 5)
  lookup-c = trans (lookup-miss σ₀ {h} {c} (nlam c) c≢h) (lookup-hit ∅ c (nvar 5))

  -- Σ has exactly these two entries.
  classify : ∀ x n → σ x ≡ just n →
             ((x ≡ h) × (n ≡ nlam c)) ⊎ ((x ≡ c) × (n ≡ nvar 5))
  classify x n eq = at-h x (x ≟ h) eq
    where
    at-c : ∀ y → Dec (y ≡ c) → σ₀ y ≡ just n →
           ((y ≡ h) × (n ≡ nlam c)) ⊎ ((y ≡ c) × (n ≡ nvar 5))
    at-c y (yes refl) e =
      inj₂ (refl , just-inj (trans (sym e) (lookup-hit ∅ c (nvar 5))))
    at-c y (no ¬q)    e with trans (sym (lookup-miss ∅ {c} {y} (nvar 5) ¬q)) e
    ...                    | ()
    at-h : ∀ y → Dec (y ≡ h) → σ y ≡ just n →
           ((y ≡ h) × (n ≡ nlam c)) ⊎ ((y ≡ c) × (n ≡ nvar 5))
    at-h y (yes refl) e = inj₁ (refl , just-inj (trans (sym e) lookup-h))
    at-h y (no ¬q)    e = at-c y (y ≟ c) (trans (sym (lookup-miss σ₀ {h} {y} (nlam c) ¬q)) e)

------------------------------------------------------------------------
-- Σ reconstructs, and nothing in it has a reference

recon-c : σ ⊢ c ⇝ var 5
recon-c = r-var lookup-c

recon-h : σ ⊢ h ⇝ lam (var 5)
recon-h = r-lam lookup-h recon-c

private
  nil-absurd : ∀ {y : Hash} → y ∈ [] → ⊥
  nil-absurd ()

  -- Every reconstruction in Σ is one of the two above, so has no references.
  no-refs : ∀ {x t} → σ ⊢ x ⇝ t → ∀ {r} → r ∈ refsT t → ⊥
  no-refs {x} d mem with classify x _ (proj₂ (⇝-∈dom d))
  ... | inj₁ (refl , _) with ⇝-func d recon-h
  ...   | refl = nil-absurd mem
  no-refs {x} d mem | inj₂ (refl , _) with ⇝-func d recon-c
  ...   | refl = nil-absurd mem

no-edge : ∀ {x r} → ¬ (σ ⊢ x ↝ r)
no-edge (t , d , mem) = no-refs d mem

------------------------------------------------------------------------
-- Σ is well-formed

wf-σ : WF σ
wf-σ = record
  { recon-total = λ {x} mem → total x (proj₁ mem) (proj₂ mem)
  ; tgt-closed  = λ _ edge → ⊥-elim (no-edge edge)
  ; ref-acyclic = λ x → acc (λ edge → ⊥-elim (no-edge edge))
  }
  where
  total : ∀ x n → σ x ≡ just n → Recon σ x
  total x n eq with classify x n eq
  ... | inj₁ (refl , _) = lam (var 5) , recon-h
  ... | inj₂ (refl , _) = var 5       , recon-c

------------------------------------------------------------------------
-- The refutation

t-closed : Closed (ref h)
t-closed = s-ref

evaluates : σ ⊢ ref h ⇓ lam (var 5)
evaluates = e-ref recon-h e-lam

value-open : ¬ Closed (lam (var 5))
value-open (s-lam (s-var p)) = absurd p
  where
  absurd : ¬ (5 < 1)
  absurd (s≤s ())

-- lem:closed-no-stuck, exactly as the paper states it, is refuted.
paper-statement-is-false :
  ¬ (∀ {σ′ t v} → WF σ′ → Closed t → σ′ ⊢ t ⇓ v → Closed v)
paper-statement-is-false paper = value-open (paper wf-σ t-closed evaluates)
